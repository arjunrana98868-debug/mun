#pragma once
#include "Committee.h"
#include <memory>
#include <map>

// MySQL Connector/C++ (JDBC API) headers
#include <mysql/jdbc.h>

#ifdef _MSC_VER
  #pragma comment(lib, "mysqlcppconn.lib")
#endif

inline unique_ptr<sql::Connection> getDBConnection() {
    try {
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        unique_ptr<sql::Connection> con(
            driver->connect("tcp://127.0.0.1:3306", "root", "admin")
        );
        con->setSchema("mun_management");
        return con;
    } catch (sql::SQLException& e) {
        cout << "\nDatabase Connection Error: " << e.what() << endl;
        return nullptr;
    }
}

inline void dbInitTables() {
    try {
        auto con = getDBConnection();
        if (!con) return;

        unique_ptr<sql::Statement> stmt(con->createStatement());
        stmt->execute(
            "CREATE TABLE IF NOT EXISTS committees ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "name VARCHAR(100) NOT NULL UNIQUE)"
        );
        stmt->execute(
            "CREATE TABLE IF NOT EXISTS portfolios ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "committee_id INT NOT NULL, "
            "name VARCHAR(100) NOT NULL, "
            "capacity INT NOT NULL, "
            "UNIQUE KEY uq_committee_portfolio (committee_id, name), "
            "FOREIGN KEY (committee_id) REFERENCES committees(id) "
            "ON DELETE CASCADE ON UPDATE CASCADE)"
        );
        stmt->execute(
            "CREATE TABLE IF NOT EXISTS allocations ("
            "id INT AUTO_INCREMENT PRIMARY KEY, "
            "portfolio_id INT NOT NULL, "
            "delegate_name VARCHAR(100) NOT NULL, "
            "UNIQUE KEY uq_portfolio_delegate (portfolio_id, delegate_name), "
            "FOREIGN KEY (portfolio_id) REFERENCES portfolios(id) "
            "ON DELETE CASCADE)"
        );
        cout << "Database connection and tables initialized.\n";
    } catch (sql::SQLException& e) {
        cout << "Database Init Error: " << e.what() << endl;
    }
}

inline void dbLoad() {
    try {
        auto con = getDBConnection();
        if (!con) return;

        committees.clear();
        map<int, int> cIndex;
        map<int, pair<int, int>> pIndex;

        unique_ptr<sql::Statement> stmt(con->createStatement());

        // 1. Load Committees
        unique_ptr<sql::ResultSet> cRes(stmt->executeQuery("SELECT id, name FROM committees ORDER BY id"));
        while (cRes->next()) {
            int id = cRes->getInt("id");
            string name = cRes->getString("name");
            cIndex[id] = (int)committees.size();
            committees.push_back(Committee(name));
        }

        // 2. Load Portfolios
        unique_ptr<sql::ResultSet> pRes(stmt->executeQuery("SELECT id, committee_id, name, capacity FROM portfolios ORDER BY id"));
        while (pRes->next()) {
            int pid = pRes->getInt("id");
            int cid = pRes->getInt("committee_id");
            string pname = pRes->getString("name");
            int cap = pRes->getInt("capacity");

            auto it = cIndex.find(cid);
            if (it == cIndex.end()) continue;

            Committee& c = committees[it->second];
            pIndex[pid] = make_pair(it->second, (int)c.portfolios.size());
            c.portfolios.push_back(Portfolio(pname, cap));
        }

        // 3. Load Allocations
        unique_ptr<sql::ResultSet> aRes(stmt->executeQuery("SELECT portfolio_id, delegate_name FROM allocations ORDER BY id"));
        while (aRes->next()) {
            int pid = aRes->getInt("portfolio_id");
            string dname = aRes->getString("delegate_name");

            auto it = pIndex.find(pid);
            if (it == pIndex.end()) continue;

            committees[it->second.first].portfolios[it->second.second].allocated.push_back(dname);
        }
    } catch (sql::SQLException& e) {
        cout << "Database Load Error: " << e.what() << endl;
    }
}

inline bool dbAddCommittee(const string& n) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("INSERT INTO committees (name) VALUES (?)")
        );
        pstmt->setString(1, n);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbRenameCommittee(const string& oldName, const string& newName) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("UPDATE committees SET name = ? WHERE name = ?")
        );
        pstmt->setString(1, newName);
        pstmt->setString(2, oldName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbDeleteCommittee(const string& n) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("DELETE FROM committees WHERE name = ?")
        );
        pstmt->setString(1, n);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbAddPortfolio(const string& commName, const string& portName, int cap) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "INSERT INTO portfolios (committee_id, name, capacity) "
                "SELECT id, ?, ? FROM committees WHERE name = ?"
            )
        );
        pstmt->setString(1, portName);
        pstmt->setInt(2, cap);
        pstmt->setString(3, commName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbUpdatePortfolio(const string& commName, const string& oldP, const string& newP, int cap) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "UPDATE portfolios SET name = ?, capacity = ? "
                "WHERE name = ? AND committee_id = (SELECT id FROM committees WHERE name = ?)"
            )
        );
        pstmt->setString(1, newP);
        pstmt->setInt(2, cap);
        pstmt->setString(3, oldP);
        pstmt->setString(4, commName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbSetCapacity(const string& commName, const string& portName, int cap) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "UPDATE portfolios SET capacity = ? "
                "WHERE name = ? AND committee_id = (SELECT id FROM committees WHERE name = ?)"
            )
        );
        pstmt->setInt(1, cap);
        pstmt->setString(2, portName);
        pstmt->setString(3, commName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbDeletePortfolio(const string& commName, const string& portName) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "DELETE FROM portfolios WHERE name = ? AND committee_id = (SELECT id FROM committees WHERE name = ?)"
            )
        );
        pstmt->setString(1, portName);
        pstmt->setString(2, commName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbAllocate(const string& commName, const string& portName, const string& delName) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "INSERT INTO allocations (portfolio_id, delegate_name) "
                "SELECT id, ? FROM portfolios WHERE name = ? AND committee_id = (SELECT id FROM committees WHERE name = ?)"
            )
        );
        pstmt->setString(1, delName);
        pstmt->setString(2, portName);
        pstmt->setString(3, commName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}

inline bool dbDeallocate(const string& commName, const string& portName, const string& delName) {
    try {
        auto con = getDBConnection();
        if (!con) return false;

        unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement(
                "DELETE FROM allocations WHERE delegate_name = ? "
                "AND portfolio_id = (SELECT id FROM portfolios WHERE name = ? AND committee_id = (SELECT id FROM committees WHERE name = ?))"
            )
        );
        pstmt->setString(1, delName);
        pstmt->setString(2, portName);
        pstmt->setString(3, commName);
        pstmt->executeUpdate();
        return true;
    } catch (sql::SQLException& e) {
        cout << "Database Error: " << e.what() << endl;
        return false;
    }
}