#!/bin/sh

gcc  rsdb_test.c ../client/rsdb.c -o rsdb_test -I/usr/local/include -L/usr/local/lib -lrocksdb
