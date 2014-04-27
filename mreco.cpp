#include "pdfile.h"
#include <mongo/client/dbclient.h>
#include <boost/program_options.hpp>
#include <unistd.h>
#include <time.h>

namespace po = boost::program_options;

class writer {
private:
  mongo::DBClientConnection _conn;
  string _coll;
  string _nid;
  string _dcoll;
  string ierr;
  string dupError;

public:
  writer(const string &target, const string &coll, string &nid);
  void save(DeletedRecord *dr);
  void save(Record *r);
};

const string currentDateTime() {
  time_t now = time(0);
  struct tm tstruct;
  char buf[80];
  tstruct = *localtime(&now);
  strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
  return buf;
}

mongo::BSONObj rename_id(const mongo::BSONObj &input, const char *newfield) {
  if (!input.hasField("_id"))
    return input;
  mongo::BSONObjBuilder builder;
  builder.appendElements(input);

  mongo::BSONElement e = input.getField("_id");
  switch (e.type()) {
  case mongo::NumberLong: {
    long long v;
    e.Val(v);
    builder.append(newfield, v);
    break;
  }
  case mongo::NumberDouble: {
    double v;
    e.Val(v);
    builder.append(newfield, v);
    break;
  }
  case mongo::NumberInt: {
    int v;
    e.Val(v);
    builder.append(newfield, v);
    break;
  }
  case mongo::Bool: {
    bool v;
    e.Val(v);
    builder.append(newfield, v);
    break;
  }
  case mongo::String: {
    string v;
    e.Val(v);
    builder.append(newfield, v);
    break;
  }
  case mongo::jstOID: {
    mongo::OID v;
    e.Val(v);
    builder.append(newfield, v);
    break;
  }
  default:
    ostringstream excep;
    excep << "unhandled ElementType for _id " << e.type() << endl;
    throw excep.str();
  }
  mongo::BSONObj tmp = builder.obj();
  return tmp.removeField("_id");
}

writer::writer(const string &target, const string &coll, string &nid)
    : _coll(coll), _dcoll(coll + "Dup"), _nid(nid), ierr("invalid bson"),
      dupError("E11000 duplicate key error") {

  try {
    _conn.connect(target.c_str());
  }
  catch (mongo::DBException & e) {
    cout << "connect to target ERROR: " << e.what() << endl;
    std::exit(1);
  }

  if (_conn.count(_coll) != 0 || _conn.count(_dcoll) != 0) {
    cout << _coll << " or " << _dcoll << " is not empty! ";
    cout << " Please choose another collect to store the data " << endl;
    std::exit(2);
  }
}

void writer::save(Record *r) {
  mongo::BSONObj o(r->data());
  _conn.insert(_coll, o);
  string err = _conn.getLastError();
  if (!err.empty()) {
    if (err.find(dupError) != string::npos) {
      if (o.hasField(_nid.c_str())) {
        cout << "some of the recorded data have " << _nid << "fileds\n";
        cout << "please used a different nid with --nid option to recover";
        cout << "exiting now.. " << endl;
        std::exit(2);
      }
      mongo::BSONObj nobj = rename_id(o, _nid.c_str());
      _conn.insert(_dcoll, nobj);
    } else if (err.find(ierr) != string::npos) {
      throw 1;
    } else {
      cout << "Inert Error " << err << endl;
      std::exit(4);
    }
  }
}

void writer::save(DeletedRecord *dr) {
  Record *nr = dr->asnormal();
  int len = nr->datalen();
  const int clen = len;
  for (int i = clen - 1; i >= 0; --i) {
    if (*(char *)(nr->data() + i) == 0) {
      len--;
    } else {
      break;
    }
  }
  reinterpret_cast<unsigned *>(nr->data())[0] = len + 1;
  try {
    save(nr);
  }
  catch (int & i) {
    if (i == 1) {
      reinterpret_cast<unsigned *>(nr->data())[0] = len + 2;
      save(nr);
    }
  }
}

int main(int argc, char **argv) {
  string dbpath, dbname, target, collection;
  string delcoll;
  // if there are more than 1 rows with the same _id, only the 1st will be
  // inserted into collection
  // the _id filed will be changed to "nid" for the rest rows, and stored in
  // "collection".Dup
  string nid("id__");
  // bool deleted = false;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "show this message and exit")(
      "deleted", "recover deleted rows")("dcoll", po::value<string>(&delcoll),
                                         "target collection for delete rows")(
      "dbpath,p", po::value<string>(&dbpath),
      "MUST, datafile directory, if directoryperdb, specify one directory "
      "once")("db", po::value<string>(&dbname),
              "MUST, database name we need to recover data from")(
      "target,t", po::value<string>(&target),
      "MUST, a temporary mongo host with default 27017 port to store the "
      "recovered data")(
      "coll,c", po::value<string>(&collection),
      "MUST, format: db.coll. a collection to store recovered data")(
      "nid,n", po::value<string>(&nid),
      "for dumplicated _id, the _id filedname will be replace with it");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") || !vm.count("dbpath") || !vm.count("db") ||
      !vm.count("target") || !vm.count("coll")) {
    cout << endl << "mreco is used to recover all the dropped collection in "
                    "the given database " << endl;
    cout << endl << desc << endl;
    cout << "You can safely ingore the message like this:" << endl;
    cout << "assertion failure in bson library: 10334 Invalid BSONObj size: "
            "1666293760 (0x00A05163) first element: : "
            "ObjectId('00feffffff00000001009400')" << endl;
    cout << "they are not normal data. maybe index record?  Since these "
            "message are printed from BSON libary, so I can't avoid them"
         << endl;
    cout << endl << "Note:  the program will recover all the data "
                    "which was dropped in the given database" << endl;

    return 0;
  }

  if (count(collection.begin(), collection.end(), '.') != 1) {
    cout << "format -c db.collection" << endl;
    return -1;
  }

  writer writer(target, collection, nid);

  if (dbpath[dbpath.size() - 1] != '/')
    dbpath.push_back('/');

  Database db(dbpath, dbname);

  time_t start = time(0);
  if (vm.count("deleted")) {
    // reocver deleted rows
    if (!vm.count("dcoll")) {
      cout << "you must specify --dcoll=collection if used --deleted" << endl;
      return -3;
    }
    delcoll = dbname + "." + delcoll;
    Collection *target = db.getns(delcoll);
    DiskLoc *del = target->firstDel();
    for (int i = 0; i < Buckets; ++i) {
      DiskLoc dl = *(del + i);
      if (dl.isNull())
        continue;
      DeletedRecord *dr = NULL;
      do {
        dl = dr->next();
        dr = db.getDelRec(dl);
        writer.save(dr);
      } while (dr->hasmore());
    }
  } else {
    // recover dropped collection
    string fl = dbname + ".$freelist";
    Collection *freelist = db.getns(fl);
    DiskLoc cur = freelist->firstExt();
    if (!cur.isNull()) {
      Extent *ext = NULL;
      do {
        cur = ext->next();
        ext = db.getExt(cur);
        DiskLoc dl = ext->firstRec();

        if (dl.isNull())
          // there is no record in this Extent
          continue;
        Record *r = NULL;
        do {
          dl = r->next(cur);
          if (dl.isNull())
            break;
          r = db.getRec(dl);
          writer.save(r);
        } while (dl != ext->lastRec());
      } while (ext->hasmore());
    }
  }
  return 0;
}
