void deleteDelegate()
{
    try
    {
        sql::mysql::MySQL_Driver* driver;
        driver = sql::mysql::get_mysql_driver_instance();
        sql::Connection* con;
        con = driver->connect(
            "tcp://127.0.0.1:3306",
            "root",
            "admin"
        );
        con->setSchema("mun_management");
        if (con == nullptr)
        {
            cout << "\nDatabase connection is not available!\n";
            return;
        }
        int id;
        cout << "\nEnter Delegate ID to delete: ";
        cin >> id;
        sql::PreparedStatement* checkStmt =
            con->prepareStatement(
                "SELECT name FROM delegates "
                "WHERE delegate_id = ?"
            );
        checkStmt->setInt(1, id);
        sql::ResultSet* res = checkStmt->executeQuery();
        if (!res->next())
        {
            cout << "\nDelegate not found!\n";
            delete res;
            delete checkStmt;
            delete con;
            return;
        }
        cout << "\nDelegate Found: "
             << res->getString("name") << endl;

        delete res;
        delete checkStmt;

        char confirm;
        cout << "\nAre you sure you want to delete this delegate? (Y/N): ";
        cin >> confirm;
        if (confirm != 'Y' && confirm != 'y')
        {
            cout << "\nDeletion cancelled.\n";
            delete con;
            return;
        }
        sql::PreparedStatement* stmt =
            con->prepareStatement(
                "DELETE FROM award "
                "WHERE delegate_id = ?"
            );
        stmt->setInt(1, id);
        stmt->executeUpdate();
        delete stmt;
        stmt =
            con->prepareStatement(
                "DELETE FROM previous_committees "
                "WHERE delegate_id = ?"
            );
        stmt->setInt(1, id);
        stmt->executeUpdate();
        delete stmt;
        stmt =
            con->prepareStatement(
                "DELETE FROM delegates "
                "WHERE delegate_id = ?"
            );
        stmt->setInt(1, id);
        int rows = stmt->executeUpdate();
        delete stmt;
        if (rows > 0)
        {
            cout << "\nDelegate deleted successfully!\n";
        }
        else
        {
            cout << "\nDelegate could not be deleted.\n";
        }
        sql::Statement* checkEmpty = con->createStatement();
        sql::ResultSet* emptyRes = checkEmpty->executeQuery("SELECT COUNT(*) AS cnt FROM delegates");
         emptyRes->next();
        int count = emptyRes->getInt("cnt");
         delete emptyRes;
         delete checkEmpty;
        if (count == 0) {
            sql::Statement* truncateStmt = con->createStatement();
            truncateStmt->execute("TRUNCATE TABLE delegates");
            delete truncateStmt;
            cout << "\nTable was empty, reset AUTO_INCREMENT to 1.\n";
        }
        delete con;
    }
    catch (sql::SQLException& e)
    {
        cout << "\nSQL Error: " << e.what() << endl;
    }
}