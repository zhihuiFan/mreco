#include "header.h"
#include <mongo/bson/bson.h>
#include <mongo/bson/bsonobj.h>
#include <mongo/bson/bsonelement.h>
#include <mongo/client/dbclient.h>

const int Buckets = 19;
size_t rownum = 1;
#pragma pack(1)
class Namespace {
 public:
  enum MaxNsLenValue {
    MaxNsLen = 128
  };

  char buf[MaxNsLen];
};

class DiskLoc {
  // private:
 public:
  enum SentinelValues {
    NullOfs = -1,
    MaxFiles = 16000
  };
  int _a;
  int ofs;

  DiskLoc(int a, int ofs) : _a(a), ofs(ofs) {}
  DiskLoc() : _a(-1), ofs(0) {}

  bool isNull() const { return _a == -1; }
  int getOfs() const { return ofs; }
  void setloc(int a, int of) {
    _a = a;
    ofs = of;
  }
  void dump() const { cout << _a << ":" << ofs << endl; }

  bool operator==(const DiskLoc &d) const { return d._a == _a && d.ofs == ofs; }
  bool operator!=(const DiskLoc &d) const { return d._a != _a || d.ofs != ofs; }
  const DiskLoc &operator=(const DiskLoc &b) {
    _a = b._a;
    ofs = b.ofs;
    return *this;
  }
};

class Record {
 private:
  int _lengthWithHeaders;
  int _extentOfs;

 public:
  int _nextOfs;
  int _prevOfs;
  char _data[4];

 public:
  const char *data() const { return _data; }
  char *data() { return _data; }
  DiskLoc nextInExtent(const DiskLoc &myLoc) {
    if (_nextOfs == DiskLoc::NullOfs) return DiskLoc();
    assert(_nextOfs);
    return DiskLoc(myLoc._a, _nextOfs);
  }
};

class Extent {
 public:
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

  Record *getRecord(DiskLoc dl) {
    assert(!dl.isNull());
    assert(dl._a == myLoc._a);
    int x = dl.getOfs() - myLoc.getOfs();
    assert(x > 0);
    return (Record *)(((char *)this) + x);
  }

  void dumpRows() {
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
        cur.setloc(cur._a, r->_nextOfs);
        continue;
      }
      cur.setloc(cur._a, r->_nextOfs);
    } while (cur != lastRecord);
  }
};

class Collection {
 public:
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

 private:
  char indexdetails[160];
  int _isCapped;
  int _maxDocInCapped;
  double _paddingFactor;
  int _systemFlags;

 public:
  DiskLoc capExtent;
  DiskLoc capFirstNewRecord;
  unsigned short dataFileVersion;
  unsigned short IndexFileVersion;
  unsigned long long multiKeyIndexBits;

 private:
  // ofs 400 (16)
  unsigned long long reservedA;
  long long extraOffset;

 public:
  int indexBuildsInProgress;

 private:
  int _userFlags;
  char reserved[72];
};

class chunk {
  // hashTable<Namespace, NamespaceDetails>::Node
  int hash;  // 0 if unused
  Namespace key;
  Collection ndetails;
};
#pragma pack()
class Database {
  // TODO:  add contructor and destructor
  // find the .freelist extention's diskloc
 public:
  Database(string db) : _db(db) {
    openAll();
    nsscan();
  }
  void openAll();
  void nsscan();  // scan db.ns file
  string &getName() { return _db; }
  Collection *getns(string ns) { return colls[ns]; }
  void getallns(vector<string> &allns);
  Extent *builtExt(DiskLoc &loc);

  vector<void *> mapfiles;
  vector<size_t> filesize;

 protected:
  string _db;

 private:
  map<string, Collection *> colls;
  void *ns;  // Point to db.ns
  size_t nslen;
  void *fmap(string &filename, size_t len);
  size_t flen(string &filename);
};

class fileGenerator {
 public:
  fileGenerator(string &db, int n) : _db(db), _n(n) {}
  void generate(vector<string> &files) {
    ostringstream f;
    for (int i = 0; i < _n; i++) {
      f.str("");
      f << _db << "." << i;
      files[i] = f.str();
    }
  }

 private:
  string _db;
  int _n;
};
