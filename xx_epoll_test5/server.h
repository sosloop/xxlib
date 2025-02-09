﻿#pragma once
#include "xx_epoll.h"
#include "xx_dict_mk.h"
#include "xx_obj.h"
#include "xx_coro.h"
#include "pkg_game_client.h"
namespace EP = xx::Epoll;

// 预声明
struct GListener;
struct GPeer;
struct VPeer;
struct Timer;
struct DBDialer;
struct DBPeer;
struct LDialer;
struct LPeer;

// 服务本体
struct Server : EP::Context {
    // 继承构造函数
    using EP::Context::Context;

    // 等待 gateway 接入的监听器
    xx::Shared<GListener> gatewayListener;

    // 用于 dialer
    xx::Shared<Timer> timer;

    // for connect to db server
    xx::Shared<DBDialer> dbDialer;
    xx::Shared<DBPeer> dbPeer;

    // for connect to lobby server
    xx::Shared<LDialer> lobbyDialer;
    xx::Shared<LPeer> lobbyPeer;

    // shared obj manager
    xx::ObjManager om;

    // gateway peers. key: gateway id
    std::unordered_map<uint32_t, xx::Shared<GPeer>> gps;

    // virtual peers( players ). key0: gateway id << 32 | client id   key1: account id (logic data)
    xx::DictMK<xx::Shared<VPeer>, uint64_t, int32_t> vps;

    // Make: accountId = --server->autoDecId
    // Kick: clientId = --server->autoDecId
    int32_t autoDecId = 0;

    // game1 scene
    xx::Shared<Game1::Scene> scene;

    // 根据 config 进一步初始化各种成员
    int Init();

    // logic here( call vps's update )
    int FrameUpdate() override;

    // 得到执行情况的快照
    std::string GetInfo();

    /**************************************************************************************/
    // caches

    // package instance cache for Send Push / Response ( can't hold )
    std::array<xx::ObjBase_s, 65536> objs{};

    // return cached package instance
    template<typename T>
    xx::Shared<T> const& FromCache() {
        assert((*(xx::Shared<T>*)&objs[xx::TypeId_v<T>]).useCount() == 1);
        return *(xx::Shared<T>*)&objs[xx::TypeId_v<T>];
    }

    /****************************************************************************************/

    // coroutine support
    xx::Coros coros;

    template<typename PKG = xx::ObjBase, typename Sender, typename Rtv, typename ... Args>
    int CoroSendRequest(Sender& sender, Rtv &rtv, Args const &... args) {
        assert(sender && sender->Alive());
        return sender->template SendRequest<PKG>([w = xx::SharedFromThis(this).ToWeak(), &rtv](int32_t const &eventKey, xx::ObjBase_s &&ob) {
            if (auto vp = w.Lock()) {
                rtv = std::move(ob);
                vp->coros.FireEvent(eventKey);
            }
        }, 99.0, args...);
    }

};
