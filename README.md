mreco
======
> A tool for recovering dropped collection or deleted rows

mreco will read mongo data file directly to recover deleted rows OR dropped collections. 
When we use mreco, we also need a mongo instance to store the recovered data.  This mongo instance need use the default port 27017 and can be connected without password.

Dependencies:
------------
1. c++ boost library 4.7+
2. mongo c++ client library


Compile:
---------
$ cd mreco  
$ scons

Usage：
-----

1. Recover deleted rows

    ```sh
    mreco --deleted --db=dbname --dcoll=dbname.collection --dbpath=/data/mongo/data -t abc.vip.xxx.com -c tdb.coll
    ```
    
    - `--deleted`:  recovered the deleted rows   
    - `--dbpath`:   the location of mongo data file,  if directoryperdb is used, we need to include db name in the dbpath also.
    - `-t xxx`:   the target mongo instance
    - `-c tdb.coll`:   the database name and collection name to store the recovered data

 
2. Recover dropped collection

    ```sh
    mreco --db=dbname  --dbpath=/data/mongo/data -t abc.vip.xxx.com -c tdb.coll2
    ```

    This will recover all the all the rows in all the dropped collection.  we can't just recover a specialized collection here(I think it is impossible here).  we have to filter out the rows we needed in all the recover data. 

Bug report:
-----------

if you find any bugs, please report to zhihuifan@163.com 

It would be my pleasure that this tool can help anyone. 


Known Issues:
-------------
1. There are some changes after r2.4.6 (73ca6bf23e0d37ee781085466df6c989558c64a3), mreco will not work for the version after r2.4.6


TODO:
-----
fix any known issues
