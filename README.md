mreco
======
> A tool for recovering dropped collection or deleted rows

mreco will read mongo data file directly to recover deleted rows OR dropped collections. 
When we use mreco, we also need a mongo instance to store the recovered data.  To make the mreco simple, the mongo instanace have to listen on default port 27017 and no password required when loggin. 

Dependencies:
------------
1. c++ boost library 1.49
2. mongo c++ client library


Compile:
---------
    $ git tag -l  (only taged version is tested, these commits after the lastest tag is still in development)
    $ git checkout v..
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

Known Issues:
-------------


TODO:
-----
fix any known issues
