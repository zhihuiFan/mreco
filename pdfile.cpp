#include "pdfile.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <boost/filesystem.hpp>
#include <fcntl.h>

DiskLoc Record::nextInExtent(const DiskLoc &myLoc) {
  if (_nextOfs == DiskLoc::NullOfs) return DiskLoc();
  assert(_nextOfs);
  return DiskLoc(myLoc.a(), _nextOfs);
}
Record *Extent::getRecord(DiskLoc dl) {
  assert(!dl.isNull());
  assert(dl.a() == myLoc.a());
  int x = dl.getOfs() - myLoc.getOfs();
  assert(x > 0);
  return (Record *)(((char *)this) + x);
}
void Extent::dumpRows() {
  DiskLoc cur = firstRecord;
  mongo::DBClientConnection target;  // we will store the recovered data here
  try {
    target.connect("phx7b01c-709495.stratus.phx.ebay.com");
  }
  catch (const mongo::DBException &e) {
    cout << "connect error " << endl;
    return;
  }
  do {
    Record *r = getRecord(cur);
    cout << "Recovered " << rownum++ << " Rows" << endl;

    try {
      mongo::BSONObj o(r->data());
      target.insert("mreco.reco", o);
    }
    catch (bson::assertion &e) {
      cout << e.what();
      cur.setloc(cur.a(), r->_nextOfs);
      continue;
    }
    cur.setloc(cur.a(), r->_nextOfs);
  } while (cur != lastRecord);
}
void *Database::fmap(string &filename, size_t len) {
  int fd = open(filename.c_str(), O_RDONLY);
  assert(fd > 0);
  void *p = mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  assert(p != MAP_FAILED);
  return p;
}

size_t Database::flen(string &filename) {
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
  int n = 11;
  filesize.resize(n);
  mapfiles.resize(n);
  vector<string> filename(n);
  fileGenerator(_db, n).generate(filename);
  for (int i = 0; i < n; ++i) {
    size_t len = flen(filename[i]);
    assert(len > 0);
    void *p = fmap(filename[i], len);
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
  string nss = _db + ".ns";
  nslen = flen(nss);
  assert(nslen > 0);
  ns = fmap(nss, nslen);
  int chunksize = sizeof(chunk);
  // char buf[chunksize];
  size_t curops = 0;
  while (curops < nslen) {
    if (curops + chunksize > nslen) {
      // TODO: why will this happen?
      return;
    }
    int hv = *(int *)(ns + curops);
    if (hv) {
      string coll((char *)ns + curops + sizeof(int), 128);
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

int main(int argc, char **argv) {
  // cout << "chunk size " << sizeof(chunk) << endl;
  Database d(argv[1]);
  // cout << d.getName() << endl;
  int numfile = d.filesize.size();
  vector<string> colls;
  d.getallns(colls);
  /*
  for(vector<string>::iterator it = colls.begin(); it!=colls.end(); it++)
      // really stupid here, but can't use c++11 now.
      cout<<"collection " <<*it<<":" << it->size()<<endl;
  */
  string f("cmiaas.$freelist");
  f.insert(f.end(), 128 - f.size(), '\0');
  Collection *freelist = d.getns(f);
  // cout << "Dump freelist of Database " << d.getName() << endl;
  DiskLoc cur = freelist->firstExt;
  // UNDERSTANDY: why the following doesn't work
  // DiskLoc& cur = freelist->firstExt;
  int i = 1;
  while (cur != freelist->lastExt) {
    // cout << "Extent " << i++ << " ";
    cur.dump();
    Extent *e = d.builtExt(cur);
    e->dumpRows();
    assert(e->myLoc == cur);
    cur = e->xnext;
  }
  // cout << "Extent " << i << " ";
  freelist->lastExt.dump();
}
