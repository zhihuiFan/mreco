#ifndef _H_PDFILE
#define _H_PDFILE

const int Buckets = 19;

#pragma pack(1)
class Namespace {
 public:
  enum MaxNsLenValue {
    MaxNsLen = 128
  };

  char buf[MaxNsLen];
};

class DiskLoc {
public:
  enum SentinelValues {
    NullOfs = -1,
    MaxFiles = 16000
  };

public:
  DiskLoc(int a, int ofs) : _a(a), ofs(ofs) {}
  DiskLoc() : _a(-1), ofs(0) {}

  const DiskLoc &operator=(const DiskLoc &b) {
    _a = b._a;
    ofs = b.ofs;
    return *this;
  }

  bool operator==(const DiskLoc &d) const { return d._a == _a && d.ofs == ofs; }
  bool operator!=(const DiskLoc &d) const { return d._a != _a || d.ofs != ofs; }

  bool isNull() const { return _a == -1; }
  int getOfs() const { return ofs; }
  int a() const { return _a; }
  void setloc(int a, int of) {
    _a = a;
    ofs = of;
  }

 private:
  int _a;
  int ofs;
};

class Record {
 private:
  int _lengthWithHeaders;
  int _extentOfs;
  int _nextOfs;
  int _prevOfs;
  char _data[4];

 public:
  const char *data() const { return _data; }
  char *data() { return _data; }
  bool hasmore(const DiskLoc &ext) const { return _nextOfs != DiskLoc::NullOfs ;}
  DiskLoc next(const DiskLoc &ext);
  int datalen() const { return _lengthWithHeaders - 16; }
};

class DeletedRecord {
 private:
  int _lengthWithHeaders;
  int _extentOfs;
  DiskLoc _nextDeleted;
  char _data[4];
 public:
  bool hasmore(const int bkt) { return ! _nextDeleted.isNull();}

};

class Extent {
 private:
  enum {
    extentSignature = 0x41424344
  };
  unsigned magic;
  DiskLoc myLoc;
  DiskLoc xnext, xprev;
  Namespace nsDiagnostic;
  int length;
  DiskLoc firstRecord;
  DiskLoc lastRecord;
  char _extentData[4];

 public:
  bool hasmore() const { return ! xnext.isNull;}
  DiskLoc next() const { return xnext; }
  DiskLoc firstRec() const { return firstRecord;}
  DiskLoc lastRec() const { return lastRecord;}
};

class Collection {
 private:
  DiskLoc firstExt;
  DiskLoc lastExt;

  DiskLoc deletedList[Buckets];
  // ofs 168 (8 byte aligned)
  struct Stats {
    // datasize and nrecords MUST Be adjacent code assumes!
    long long datasize;  // this includes padding, but not record headers
    long long nrecords;
  } stats;
  int lastExtentSize;
  int nIndexes;

  char indexdetails[160];
  int _isCapped;
  int _maxDocInCapped;
  double _paddingFactor;
  int _systemFlags;

  DiskLoc capExtent;
  DiskLoc capFirstNewRecord;
  unsigned short dataFileVersion;
  unsigned short IndexFileVersion;
  unsigned long long multiKeyIndexBits;

  // ofs 400 (16)
  unsigned long long reservedA;
  long long extraOffset;

  int indexBuildsInProgress;

  int _userFlags;
  char reserved[72];
 public:
  DiskLoc firstExt() { return firstExt; }
  DiskLoc* firstDel() { return deletedList; }

};

class chunk {
  // hashTable<Namespace, NamespaceDetails>::Node
  int hash;  // 0 if unused
  Namespace key;
  Collection ndetails;
};
#pragma pack()
class Database {
 public:
  Database(string &path, string &db) : _path(path), _db(db) {
    openAll();
    nsscan();
  }
  string &getName() { return _db; }
  Collection *getns(string ns) { return colls[ns]; }
  Extent *getExt(const DiskLoc &loc);
  Record *getRec(const DiskLoc &loc);

 private:
  void *fmap(const string &filename, size_t len);
  size_t flen(const string &filename);
  void openAll();
  void nsscan();  // scan db.ns file

 private:
  string _db;
  string _path;
  map<string, Collection *> colls;
  void *ns;  // Point to db.ns
  vector<void *> mapfiles;
  vector<size_t> filesize;

};
#endif
