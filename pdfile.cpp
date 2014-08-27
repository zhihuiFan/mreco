#include "pdfile.h"
#include "header.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <dirent.h>

Database::Database(string &path, string &db) : _path(path), _db(db) {
  openAll();
  nsscan();
  initFreelistLoc();
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
    std::exit(-100);
  }
  return len;
}

void Database::openAll() {
  ostringstream o;
  o << "(^" << _db << "\\.[0-9]+)";
  const boost::regex reg(o.str().c_str());
  DIR *path = opendir(_path.c_str());
  struct dirent *subfile;
  int n = 0;
  if (path) {
    while ((subfile = readdir(path)) != NULL) {
      if (subfile->d_type == DT_REG) {
        if (boost::regex_search(subfile->d_name, reg)) n++;
      }
    }
  } else {
    cout << "can open file " << _path.c_str() << " exit.." << std::endl;
    exit(1);
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
    mapfiles.push_back(p);
    filesize.push_back(len);
  }
}

void Database::nsscan() {
  string nss = _path + _db + ".ns";
  size_t nslen = flen(nss);
  void *ns = fmap(nss, nslen);
  int chunksize = sizeof(chunk);
  size_t curops = 0;
  while (curops < nslen) {
    if (curops + chunksize > nslen) {
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

void Database::initFreelistLoc() {
  string fl = _db + ".$freelist";
  Collection *freelist = this->getns(fl);
  if (freelist) {
    freelist_start_ = freelist->firstExt();
    freelist_end_ = freelist->lastExt();
  } else {
    freelist_start_ = ((DataFileHeader *)mapfiles[0])->freeListStart;
    freelist_end_ = ((DataFileHeader *)mapfiles[0])->freeListEnd;
  }
}

Extent *Database::getExt(const DiskLoc &loc) const {
  return (Extent *)(mapfiles[loc.a()] + loc.getOfs());
}

Record *Database::getRec(const DiskLoc &loc) const {
  return (Record *)(mapfiles[loc.a()] + loc.getOfs());
}

DeletedRecord *Database::getDelRec(const DiskLoc &loc) const {
  return (DeletedRecord *)(mapfiles[loc.a()] + loc.getOfs());
}
