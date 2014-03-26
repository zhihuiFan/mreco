#include "pdfile.h"
#include <mongo/client/dbclient.h>
#include <boost/program_options.hpp>
#include <unistd.h>
#include <time.h>

namespace po = boost::program_options;

const string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

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
      cout << endl << "mreco is used to recover all the dropped collection in the given database " << endl;
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
    cerr << collection << " or " << collDup << " is not empty! ";
    cerr << " Please choose another collect to store the data " << endl;
    return -2;
  }

  time_t start = time(0);
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
            if(it->hasField(nid.c_str())){
                cout << "some of the recorded data have " << nid << "fileds\n";
                cout << "please used a different nid with --nid option to recover";
                cout << "exiting now.. " << endl;
                return -3;
            }
          mongo::BSONObj nobj = rename_id(*it, nid.c_str());
        } else {
          cerr << "insert Error " << err << endl;
          return -4;
        }
      }
    }
    cout << currentDateTime() <<  " Recovered " << data.size() << " rows in this extent " << endl;
    cur = e->xnext;
  } while (cur != freelist->lastExt);

  time_t end = time(0);
  size_t dups = tconn.count(collDup);
  cout << "Recover completed, it recovered " << tconn.count(collection) + dups << " rows in total in " << end-start << " seconds" << endl;
  cout << "Please check collection " << collection << " and " << collDup << " for details " << endl;
  cout << "If the ids is duplicated, the later records will be stored in " << collDup  << " with its fieldName '_id' changed to " << nid << endl;
  cout << "The real value of _id for these duplicated rows is generated by c++ driver automatically" << endl;
  return 0;
}
