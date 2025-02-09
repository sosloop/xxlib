﻿#pragma once

#include "xx_helpers.h"

// 类似 std::shared_ptr / weak_ptr，非线程安全，Weak 提供了无损 useCount 检测功能以方便直接搞事情

namespace xx {

    /************************************************************************************/
    // Make 时会在内存块头部附加

    struct PtrHeaderBase {
        uint32_t useCount;      // 强引用技术
        uint32_t refCount;      // 弱引用技术
    };


    // 适配路由
    template<typename T, typename ENABLED = void>
    struct PtrHeaderSwitcher {
        using type = PtrHeaderBase;
    };

    template<typename T, typename ENABLED = void>
    using PtrHeader_t = typename PtrHeaderSwitcher<T>::type;


    /************************************************************************************/

    // for ObjBase 序列化等需求
    struct PtrHeader : PtrHeaderBase {
        union {
            struct {
                uint32_t typeId;        // 序列化 或 类型转换用
                uint32_t offset;        // 序列化等过程中使用
            };
            void* ud;
        };
    };


    /************************************************************************************/
    // std::shared_ptr like

    template<typename T>
    struct Weak;

    template<typename T>
    struct Ptr;


    template<typename T>
    struct Shared {
        using HeaderType = PtrHeader_t<T>;
        using ElementType = T;
        T *pointer = nullptr;

        XX_INLINE operator T *const &() const noexcept {
            return pointer;
        }

        XX_INLINE operator T *&() noexcept {
            return pointer;
        }

        XX_INLINE T *const &operator->() const noexcept {
            return pointer;
        }

        XX_INLINE T const &Value() const noexcept {
            return *pointer;
        }

        XX_INLINE T &Value() noexcept {
            return *pointer;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE explicit operator bool() const noexcept {
            return pointer != nullptr;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE bool Empty() const noexcept {
            return pointer == nullptr;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE bool HasValue() const noexcept {
            return pointer != nullptr;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE uint32_t useCount() const noexcept {
            if (!pointer) return 0;
            return header()->useCount;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE uint32_t refCount() const noexcept {
            if (!pointer) return 0;
            return header()->refCount;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE uint32_t typeId() const noexcept {
            if (!pointer) return 0;
            assert(header()->typeId);
            return header()->typeId;
        }

        // unsafe
        [[maybe_unused]] [[nodiscard]] XX_INLINE HeaderType *header() const noexcept {
            return ((HeaderType *) pointer - 1);
        }

        // 将 header 内容拼接为 string 返回 以方便显示 & 调试
        [[maybe_unused]] [[nodiscard]] XX_INLINE std::string GetHeaderInfo() const noexcept {
            std::string s;
            if (!pointer) {
                s += "nil";
            } else {
                auto h = header();
                s += "{\"useCount\":" + std::to_string(h->useCount);
                s += ",\"refCount\":" + std::to_string(h->refCount);
                if constexpr (std::is_base_of_v<PtrHeader, HeaderType>) {
                    s += ",\"typeId\":" + std::to_string(h->typeId);
                    s += ",\"offset\":" + std::to_string(h->offset);
                }
                s += "}";
            }
            return s;
        }

        void Reset() {
            if (pointer) {
                auto h = header();
                assert(h->useCount);
                // 不能在这里 -1, 这将导致成员 weak 指向自己时触发 free
                if (h->useCount == 1) {
                    pointer->~T();
                    pointer = nullptr;
                    if (h->refCount == 0) {
                        free(h);
                    } else {
                        h->useCount = 0;
                    }
                } else {
                    --h->useCount;
                    pointer = nullptr;
                }
            }
        }

        template<typename U>
        void Reset(U *const &ptr) {
            static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);
            if (pointer == ptr) return;
            Reset();
            if (ptr) {
                pointer = ptr;
                ++((HeaderType *) ptr - 1)->useCount;
            }
        }

        XX_INLINE ~Shared() {
            Reset();
        }

        Shared() = default;

        template<typename U>
        XX_INLINE Shared(U *const &ptr) {
            static_assert(std::is_base_of_v<T, U>);
            pointer = ptr;
            if (ptr) {
                ++((HeaderType *) ptr - 1)->useCount;
            }
        }

        XX_INLINE Shared(T *const &ptr) {
            pointer = ptr;
            if (ptr) {
                ++((HeaderType *) ptr - 1)->useCount;
            }
        }

        template<typename U>
        XX_INLINE Shared(Shared<U> const &o) : Shared(o.pointer) {}

        XX_INLINE Shared(Shared const &o) : Shared(o.pointer) {}

        template<typename U>
        XX_INLINE Shared(Shared<U> &&o) noexcept {
            static_assert(std::is_base_of_v<T, U>);
            pointer = o.pointer;
            o.pointer = nullptr;
        }

        XX_INLINE Shared(Shared &&o) noexcept {
            pointer = o.pointer;
            o.pointer = nullptr;
        }

        template<typename U>
        XX_INLINE Shared &operator=(U *const &ptr) {
            static_assert(std::is_base_of_v<T, U>);
            Reset(ptr);
            return *this;
        }

        XX_INLINE Shared &operator=(T *const &ptr) {
            Reset(ptr);
            return *this;
        }

        template<typename U>
        XX_INLINE Shared &operator=(Shared<U> const &o) {
            static_assert(std::is_base_of_v<T, U>);
            Reset(o.pointer);
            return *this;
        }

        XX_INLINE Shared &operator=(Shared const &o) {
            Reset(o.pointer);
            return *this;
        }

        template<typename U>
        XX_INLINE Shared &operator=(Shared<U> &&o) {
            static_assert(std::is_base_of_v<T, U>);
            Reset();
            std::swap(pointer, (*(Shared *) &o).pointer);
            return *this;
        }

        XX_INLINE Shared &operator=(Shared &&o) {
            std::swap(pointer, o.pointer);
            return *this;
        }

        template<typename U>
        XX_INLINE bool operator==(Shared<U> const &o) const noexcept {
            return pointer == o.pointer;
        }

        template<typename U>
        XX_INLINE bool operator!=(Shared<U> const &o) const noexcept {
            return pointer != o.pointer;
        }

        // 有条件的话尽量使用 ObjManager 的 As, 避免发生 dynamic_cast
        template<typename U>
        XX_INLINE Shared<U> As() const noexcept {
            if constexpr (std::is_same_v<U, T>) {
                return *this;
            } else if constexpr (std::is_base_of_v<U, T>) {
                return pointer;
            } else {
                return dynamic_cast<U *>(pointer);
            }
        }

        // unsafe: 直接硬转返回. 使用前通常会根据 typeId 进行合法性检测
        template<typename U>
        XX_INLINE Shared<U> &ReinterpretCast() const noexcept {
            return *(Shared<U> *) this;
        }

        struct Weak<T> ToWeak() const noexcept;

        // 填充式 make
        template<typename...Args>
        Shared &Emplace(Args &&...args);

        // singleton convert to std::shared_ptr ( usually for thread safe )
        std::shared_ptr<T> ToSharedPtr() noexcept {
            assert(useCount() == 1 && refCount() == 0);
            auto bak = pointer;
            pointer = nullptr;
            return std::shared_ptr<T>(bak, [](T *p) {
                p->~T();
                free((HeaderType *) p - 1);
            });
        }
    };

    /************************************************************************************/
    // std::weak_ptr like

    template<typename T>
    struct Weak {
        using HeaderType = PtrHeader_t<T>;
        using ElementType = T;
        HeaderType *h = nullptr;

        [[maybe_unused]] [[nodiscard]] XX_INLINE uint32_t useCount() const noexcept {
            if (!h) return 0;
            return h->useCount;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE uint32_t refCount() const noexcept {
            if (!h) return 0;
            return h->refCount;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE uint32_t typeId() const noexcept {
            if (!h) return 0;
            return h->typeId;
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE explicit operator bool() const noexcept {
            return h && h->useCount;
        }

        // unsafe: 直接计算出指针
        [[maybe_unused]] [[nodiscard]] XX_INLINE T *pointer() const {
            return (T *) (h + 1);
        }

        XX_INLINE void Reset() {
            if (h) {
                if (h->refCount == 1 && h->useCount == 0) {
                    free(h);
                } else {
                    --h->refCount;
                }
                h = nullptr;
            }
        }

        template<typename U>
        XX_INLINE void Reset(Shared<U> const &s) {
            static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);
            Reset();
            if (s.pointer) {
                h = ((HeaderType *) s.pointer - 1);
                ++h->refCount;
            }
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE Shared<T> Lock() const {
            if (h && h->useCount) {
                auto p = h + 1;
                return *(Shared<T> *) &p;
            }
            return {};
        }

        // unsafe 系列: 要安全使用，每次都 if 真 再调用这些函数 1 次。一次 if 多次调用的情景除非很有把握在期间 Shared 不会析构，否则还是 Lock()
        [[maybe_unused]] [[nodiscard]] XX_INLINE ElementType *operator->() const noexcept {
            return (ElementType *) (h + 1);
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE ElementType const &Value() const noexcept {
            return *(ElementType *) (h + 1);
        }

        [[maybe_unused]] [[nodiscard]] XX_INLINE ElementType &Value() noexcept {
            return *(ElementType *) (h + 1);
        }

        XX_INLINE operator ElementType *() const noexcept {
            return (ElementType *) (h + 1);
        }

        template<typename U>
        XX_INLINE Weak &operator=(Shared<U> const &o) {
            static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);
            Reset(o);
            return *this;
        }

        template<typename U>
        XX_INLINE Weak(Shared<U> const &o) {
            static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);
            Reset(o);
        }

        XX_INLINE ~Weak() {
            Reset();
        }

        Weak() = default;

        XX_INLINE Weak(Weak const &o) {
            if ((h = o.h)) {
                ++o.h->refCount;
            }
        }

        template<typename U>
        XX_INLINE Weak(Weak<U> const &o) {
            static_assert(std::is_base_of_v<T, U>);
            if ((h = o.h)) {
                ++o.h->refCount;
            }
        }

        XX_INLINE Weak(Weak &&o) noexcept {
            h = o.h;
            o.h = nullptr;
        }

        template<typename U>
        XX_INLINE Weak(Weak<U> &&o) noexcept {
            static_assert(std::is_base_of_v<T, U>);
            h = o.h;
            o.h = nullptr;
        }

        XX_INLINE Weak &operator=(Weak const &o) {
            if (&o != this) {
                Reset(o.Lock());
            }
            return *this;
        }

        template<typename U>
        XX_INLINE Weak &operator=(Weak<U> const &o) {
            static_assert(std::is_base_of_v<T, U>);
            if ((void *) &o != (void *) this) {
                Reset(((Weak *) (&o))->Lock());
            }
            return *this;
        }

        XX_INLINE Weak &operator=(Weak &&o) noexcept {
            std::swap(h, o.h);
            return *this;
        }
        // operator=(Weak&& o) 没有模板实现，因为不确定交换 h 之后的类型是否匹配

        template<typename U>
        XX_INLINE bool operator==(Weak<U> const &o) const noexcept {
            return h == o.h;
        }

        template<typename U>
        XX_INLINE bool operator!=(Weak<U> const &o) const noexcept {
            return h != o.h;
        }
    };

    template<typename T>
    Weak<T> Shared<T>::ToWeak() const noexcept {
        if (pointer) {
            auto h = (HeaderType *) pointer - 1;
            return *(Weak<T> *) &h;
        }
        return {};
    }

    template<typename T>
    template<typename...Args>
    Shared<T> &Shared<T>::Emplace(Args &&...args) {
        Reset();
        auto h = (HeaderType *) malloc(sizeof(HeaderType) + sizeof(T));
        h->useCount = 1;
        h->refCount = 0;
        if constexpr (std::is_base_of_v<PtrHeader, HeaderType>) {
            h->typeId = TypeId_v<T>;
            h->offset = 0;
        }
        pointer = new(h + 1) T(std::forward<Args>(args)...);
        return *this;
    }


    /************************************************************************************/
    // helpers

    template<typename T>
    struct IsShared : std::false_type {
    };
    template<typename T>
    struct IsShared<Shared<T>> : std::true_type {
    };
    template<typename T>
    struct IsShared<Shared<T> &> : std::true_type {
    };
    template<typename T>
    struct IsShared<Shared<T> const &> : std::true_type {
    };
    template<typename T>
    constexpr bool IsShared_v = IsShared<T>::value;


    template<typename T>
    struct IsWeak : std::false_type {
    };
    template<typename T>
    struct IsWeak<Weak<T>> : std::true_type {
    };
    template<typename T>
    struct IsWeak<Weak<T> &> : std::true_type {
    };
    template<typename T>
    struct IsWeak<Weak<T> const &> : std::true_type {
    };
    template<typename T>
    constexpr bool IsWeak_v = IsWeak<T>::value;


    template<typename T, typename...Args>
    [[maybe_unused]] [[nodiscard]] Shared<T> Make(Args &&...args) {
        Shared<T> rtv;
        rtv.Emplace(std::forward<Args>(args)...);
        return rtv;
    }

    template<typename T, typename ...Args>
    Shared<T> TryMake(Args &&...args) noexcept {
        try {
            return Make<T>(std::forward<Args>(args)...);
        }
        catch (...) {
            return Shared<T>();
        }
    }

    template<typename T, typename ...Args>
    Shared<T> &MakeTo(Shared<T> &v, Args &&...args) {
        v = Make<T>(std::forward<Args>(args)...);
        return v;
    }

    template<typename T, typename ...Args>
    Shared<T> &TryMakeTo(Shared<T> &v, Args &&...args) noexcept {
        v = TryMake<T>(std::forward<Args>(args)...);
        return v;
    }

    template<typename T, typename U>
    Shared<T> As(Shared<U> const &v) noexcept {
        return v.template As<T>();
    }

    template<typename T, typename U>
    bool Is(Shared<U> const &v) noexcept {
        return !v.template As<T>().Empty();
    }

    // unsafe
    template<typename T>
    Shared<T> SharedFromThis(T *const &thiz) {
        return *(Shared<T> *) &thiz;
    }



    // 针对跨线程安全访问需求，将 pointer 持有+1 并塞入 std::shared_ptr，参数为安全删除 lambda
    // 令 std::shared_ptr 在最终销毁时，将指针( xx::Shared 的原始形态 ) 封送到安全线程操作
    // 最终利用 o 的析构来安全删除 pointer ( 可能还有别的持有 )
    // 下面的封装 令 std::shared_ptr<T*> 用起来更友善
    /* 示例：下列代码可能存在于主线程环境类中

        // 共享：加持 & 封送
        template<typename T>
        xx::Ptr<T> ToPtr(xx::Shared<T> const& s) {
            return xx::Ptr<T>(s, [this](T **p) { Dispatch([p] { xx::Shared<T> o; o.pointer = *p; }); });
        }

        // 如果独占：不加持 不封送 就地删除
        template<typename T>
        xx::Ptr<T> ToPtr(xx::Shared<T> && s) {
            if (s.header()->useCount == 1) return xx::Ptr<T>(std::move(s), [this](T **p) { xx::Shared<T> o; o.pointer = *p; });
            else return xx::Ptr<T>(s, [this](T **p) { Dispatch([p] { xx::Shared<T> o; o.pointer = *p; }); });
        }

    */
    template<typename T>
    struct Ptr {
        Ptr(Ptr const&) = default;
        Ptr(Ptr &&) = default;
        Ptr& operator=(Ptr const&) = default;
        Ptr& operator=(Ptr &&) = default;

        std::shared_ptr<T*> ptr;

        template<typename F>
        Ptr(Shared<T> const& s, F &&f) {
            assert(s);
            ++s.header()->useCount;
            ptr = std::shared_ptr<T*>(new T *(s.pointer), std::forward<F>(f));
        }

        template<typename F>
        Ptr(Shared<T> && s, F &&f) {
            assert(s);
            ptr = std::shared_ptr<T*>(new T *(s.pointer), std::forward<F>(f));
            s.pointer = nullptr;
        }

        XX_INLINE operator T *const &() const noexcept {
            return *ptr;
        }

        XX_INLINE operator T *&() noexcept {
            return *ptr;
        }

        XX_INLINE T *const &operator->() const noexcept {
            return *ptr;
        }
    };

}

// 令 Shared Weak 支持放入 hash 容器
namespace std {
    template<typename T>
    struct hash<xx::Shared<T>> {
        size_t operator()(xx::Shared<T> const &v) const {
            return (size_t) v.pointer;
        }
    };

    template<typename T>
    struct hash<xx::Weak<T>> {
        size_t operator()(xx::Weak<T> const &v) const {
            return (size_t) v.h;
        }
    };
}
