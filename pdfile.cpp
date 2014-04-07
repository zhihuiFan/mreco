#include "pdfile.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

DiskLoc Record::nextInExtent(const DiskLoc &myLoc) {
  if (_nextOfs == DiskLoc::NullOfs) return DiskLoc();
  assert(_nextOfs);
  return DiskLoc(myLoc.a(), _nextOfs);
}
Record *Extent::getRecord(DiskLoc dl) {
  if (dl.getOfs() == DiskLoc::NullOfs || dl.isNull()) return NULL;
  assert(dl.a() == myLoc.a());
  int x = dl.getOfs() - myLoc.getOfs();
  assert(x > 0);
  return (Record *)(((char *)this) + x);
}
void Extent::dumpRows(list<mongo::BSONObj> &store) {
  DiskLoc cur = firstRecord;
  do {
    Record *r = getRecord(cur);
    if (r == NULL) break;

    try {
      mongo::BSONObj o(r->data());
      store.push_back(o);
    }
    catch (bson::assertion &e) {
      // cout << e.what();
      cur.setloc(cur.a(), r->_nextOfs);
      continue;
    }
    if (cur == lastRecord) break;
    cur.setloc(cur.a(), r->_nextOfs);
  } while (1);
}

void *Database::fmap(const string &filename, size_t len) {
  int fd = open(filename.c_str(), O_RDWR);
  assert(fd > 0);
  void *p = mmap(NULL, len, PROT_WRITE, MAP_PRIVATE, fd, 0);
  close(fd);
  assert(p != MAP_FAILED);
  return p;
}

size_t Database::flen(const string &filename) {
  size_t len;
  try {
    len = boost::filesystem::file_size(filename);
  }
  catch (exception &e) {
    cout << "file size error " << filename << " " << e.what() << endl;
    return 0;
  }
  return len;
}

void Database::openAll() {
  ostringstream o;
  o << "(^" << _db << "\\.[0-9]+)";
  const boost::regex reg(o.str().c_str());

  boost::filesystem::path Path(_path.c_str());
  boost::filesystem::directory_iterator end;

  int n = 0;
  for (boost::filesystem::directory_iterator i(Path); i != end; i++) {
    const char *filename = i->path().filename().string().c_str();
    if (boost::regex_search(filename, reg)) n++;
  }
  filesize.reserve(n);
  mapfiles.reserve(n);
  for (int i = 0; i < n; ++i) {
    ostringstream format;
    format << _path << _db << "." << i;
    string fullname = format.str();
    size_t len = flen(fullname);
    assert(len > 0);
    void *p = fmap(fullname, len);
    mapfiles[i] = p;
    filesize[i] = len;
  }
}

void Database::getallns(vector<string> &v) {
  v.reserve(colls.size());
  for (map<string, Collection *>::iterator it = colls.begin();
       it != colls.end(); it++)
    v.push_back(it->first);
}

void Database::nsscan() {
  string nss = _path + _db + ".ns";
  size_t nslen = flen(nss);
  assert(nslen > 0);
  ns = fmap(nss, nslen);
  int chunksize = sizeof(chunk);
  size_t curops = 0;
  while (curops < nslen) {
    if (curops + chunksize > nslen) {
      // TODO: why will this happen?
      return;
    }
    int hv = *(int *)(ns + curops);
    if (hv) {
      string coll((char *)ns + curops + sizeof(int));
      Collection *nsp = (Collection *)((char *)ns + curops + sizeof(int) + 128);
      colls.insert(pair<string, Collection *>(coll, nsp));
    }
    curops = curops + chunksize;
  }
}

Extent *Database::builtExt(DiskLoc &loc) {
  assert(mapfiles[loc.a()]);
  return (Extent *)(mapfiles[loc.a()] + loc.getOfs());
}
