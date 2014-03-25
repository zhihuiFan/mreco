#include "pdfile.h"
#include <mongo/client/dbclient.h>
#include <boost/program_options.hpp>
#include <unistd.h>

namespace po = boost::program_options;

int main(int argc, char **argv) {
  string dbpath, dbname, target, collection;
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
      "MUST, format: db.coll. a collection to store recovered data");

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
  do {
    list<mongo::BSONObj> data;
    Extent *e = db.builtExt(cur);
    e->dumpRows(data);
    list<mongo::BSONObj>::iterator end = data.end();
    for (list<mongo::BSONObj>::iterator it = data.begin(); it != end; it++) {
      tconn.insert(collection.c_str(), *it);
    }
    cout << "Recovered " << data.size() << " rows in this extent " << endl;
    cur = e->xnext;
  } while (cur != freelist->lastExt);

  return 0;
}
