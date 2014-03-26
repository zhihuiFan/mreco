#include "pdfile.h"
#include <mongo/client/dbclient.h>
#include <boost/program_options.hpp>
#include <unistd.h>

namespace po = boost::program_options;

mongo::BSONObj rename_id(mongo::BSONObj &input, const char *newfield) {
  if (!input.hasField("_id")) return input;
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

int main(int argc, char **argv) {
  string dbpath, dbname, target, collection;
  // if there are more than 1 rows with the same _id, only the 1st will be
  // inserted into collection
  // the _id filed will be changed to "nid" for the rest rows, and stored in
  // "collection".Dup
  string nid("id__");
  // bool deleted = false;

  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "show this message and exit")(
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
    cout << endl << desc << endl;
    cout << "You can safely ingore the message like this:" << endl;
    cout << "assertion failure in bson library: 10334 Invalid BSONObj size: "
            "1666293760 (0x00A05163) first element: : "
            "ObjectId('00feffffff00000001009400')" << endl;
    cout << "they are not normal data. maybe index record?  Since these "
            "message are printed from BSON libary, so I can't avoid them"
         << endl;
    cout << endl << "Note:  the program will recover all the data "
                    "which was dropped in the given database,  it have "
                    "nothing with the --coll option" << endl;
    return 0;
  }

  if (count(collection.begin(), collection.end(), '.') != 1) {
    cerr << "format -c db.collection" << endl;
    return -1;
  }

  mongo::DBClientConnection tconn;
  try {
    tconn.connect(target.c_str());
  }
  catch (mongo::DBException &e) {
    cout << "connect to target ERROR: " << e.what() << endl;
    return -1;
  }

  if (dbpath[dbpath.size() - 1] != '/') dbpath.push_back('/');

  Database db(dbpath, dbname);
  string fl = dbname + ".$freelist";
  Collection *freelist = db.getns(fl);

  DiskLoc cur = freelist->firstExt;

  string dupError("E11000 duplicate key error");
  string collDup = collection + "Dup";

  if (tconn.count(collection) != 0 || tconn.count(collDup) != 0) {
    cerr << collection << " or " << collDup << " is not empty " << endl;
    return -2;
  }

  do {
    list<mongo::BSONObj> data;
    Extent *e = db.builtExt(cur);
    e->dumpRows(data);
    list<mongo::BSONObj>::iterator end = data.end();
    for (list<mongo::BSONObj>::iterator it = data.begin(); it != end; it++) {
      tconn.insert(collection.c_str(), *it);
      string err = tconn.getLastError();
      if (!err.empty()) {
        if (err.find(dupError) != string::npos) {
          mongo::BSONObj nobj = rename_id(*it, nid.c_str());
        } else {
          cerr << "insert Error " << err << endl;
          return -3;
        }
      }
    }
    cout << "Recovered " << data.size() << " rows in this extent " << endl;
    cur = e->xnext;
  } while (cur != freelist->lastExt);

  return 0;
}
