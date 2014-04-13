#ifndef _H_MRECO
#define _H_MRECO
#include <mongo/client/dbclient.h>
#include "pdfile.h"

mongo::BSONObj rename_id(const mongo::BSONObj &input, const char *newfield);

class writer{
    private:
        mongo::DBClientConnection _conn;
        string _coll;
        string _nid;
        string _dcoll; 
    public:
        writer(const string& target, const string& coll, string& nid);
        void save(const list<mongo::BSONObj> & data);
        void save(const mongo::BSONObj& data);
        size_t nwrited(bool duplicated){
            if (duplicated)
                return _conn.count(_dcoll);
            return _conn.count(_coll);
        }
};

const string currentDateTime() ;

#endif
