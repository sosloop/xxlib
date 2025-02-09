﻿#include "db.h"
#include "xx_logger.h"

DB::DB(Server *server) : server(server) {
    tp.envs[0].db = this;
    tp.envs[0].server = server;
    tp.envs[0].tp = &tp;
}

void DB::Env::operator()(std::function<void(DB::Env &)> &job) {
    try {
        job(*this);
    }
    catch (int const &e) {
        LOG_ERR("catch (int const& e) e = ", e);
    }
    catch (std::exception const &e) {
        LOG_ERR("std::exception const& e", e.what());
    }
    catch (...) {
        LOG_ERR("catch (...)");
    }
}

/****************************************************************************************/

DB::Env::Env() {
    conn.Emplace("data.db3");
    if (!*conn) {
        throw std::runtime_error("the database file can't open.");
    }
    if (conn->GetTableCount() == 0) {
        conn->Call(R"#(
CREATE TABLE acc (
    id              int         primary key     not null,
    username        text                        not null,
    password        text                        not null,
    nickname        text                        not null,
    coin            float                       not null
)
)#");
    }
    if (conn->Execute<int64_t>("select count(*) from acc") == 0) {
        conn->Call(R"#(
insert into acc(id, username, password, nickname, coin) values (1, "a", "a", "player1", 100)
)#");
        conn->Call(R"#(
insert into acc(id, username, password, nickname, coin) values (2, "b", "b", "player2", 200)
)#");
    }
}

DB::Rtv<DB::AccountInfo> DB::Env::TryGetAccountInfoByUsernamePassword(std::string_view const &username, std::string_view const &password) {
    LOG_INFO("TryGetAccountInfoByUsernamePassword username = ", username, " password = ", password);
    Rtv<AccountInfo> rtv;
    try {
        // create query
        auto &q = qTryGetAccountInfoByUsernamePassword;
        if (!q) {
            q.Emplace(*conn, "select id, nickname, coin from acc where username = ? and password = ?");
        }

        // set args
        q->SetParameters(username, password);

        // execute, fill result
        if (!q->ExecuteTo(rtv.value.accountId, rtv.value.nickname, rtv.value.coin)) {

            // not found data
            rtv.value.accountId = -1;
        }
    }
    catch(...) {
        rtv.success = false;
        rtv.errorCode = conn->lastErrorCode;
        rtv.errorMessage = conn->lastErrorMessage;
    }
    LOG_INFO("rtv.success = ", rtv.success);
    return rtv;
}
