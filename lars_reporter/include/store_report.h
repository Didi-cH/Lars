#pragma once

#include "mysql.h"
#include "lars.pb.h"

void *store_main(void *args);

class StoreReport
{
public:
    StoreReport();

    void store(lars::ReportStatusRequest &req);

private:
    MYSQL _db_conn;
};