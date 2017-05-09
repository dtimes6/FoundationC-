#ifndef  bits_hh
#define  bits_hh

#include <cassert>

struct Bits {
    typedef unsigned long long T;
    enum {
        cbitsT = sizeof(T) * 8,
    };

    struct Ls {
        virtual void   init(bool v)  = 0;

        virtual bool   full()  const = 0;
        virtual bool   empty() const = 0;

        virtual size_t first() const = 0;
        virtual bool   has(size_t i) const = 0;
        virtual void   set(size_t i) = 0;
        virtual void   unset(size_t i) = 0;
        virtual ~Ls() {}
    };

    struct L0 : public Ls {
        enum {
            ctotalT = cbitsT * cbitsT,
        };
        T* mem;
        size_t count;
            L0(): mem(NULL), count(0) {}
        void init(bool v) {
            mem = new T [cbitsT];
            memset(mem, v ? -1 : 0, sizeof(T) * cbitsT);
        }
        ~L0() { delete [] mem; }
        bool full()  const { return count == ctotalT; }
        bool empty() const { return count == 0; }

#define MK_BYTEMASK                 \
        T& byte = mem[i / cbitsT];  \
        T  mask = (T)1 << (i % cbitsT);

        bool has(size_t i) const { MK_BYTEMASK; return byte & mask; }
        inline size_t _first(size_t sz = cbitsT) const {
            assert(!empty());
            for (size_t i = 0; i < sz; ++i) {
                if (mem[i]) {
                     size_t b = 0;
                     while ((mem[i] & (1 << b)) == 0)
                        ++b;
                     return cbitsT * i + b;
                }
            }
            return ~(size_t)0;
        }
        size_t first() const { return _first(); }
        void set(size_t i) {
            assert(i < ctotalT);
            MK_BYTEMASK;
            if (byte & mask) return;
            byte |= mask;
            ++count;
        }

        void unset(size_t i) {
            assert(i < ctotalT);
            MK_BYTEMASK;
            if (!(byte & mask)) return;
            byte &= ~mask;
            --count;
        }
    };

    struct L0h : public L0 {
        size_t size;
        L0h(size_t max) : size(max), L0() {}

#define MK_SZ \
        size_t sz = (size + cbitsT - 1) / cbitsT;

        void init(bool v) {
            MK_SZ;
            mem = new T [sz];
            memset(mem, v ? -1 : 0, sizeof(T) * sz);
        }

        bool   full() const { return count == size; }
        size_t first() const { MK_SZ; return _first(sz); }
    };
    
    struct Ln : public Ls {
        typedef Ls* LsP;
        static const LsP FULL;

        size_t sizeSub;
        Ls**   mem;
        size_t fullcount;
        size_t emptycount;

        Ln(size_t sz) : sizeSub(sz), mem(NULL), fullcount(0), emptycount(cbitsT) {}
        ~Ln() { _free(); }
        void _free(size_t sz = cbitsT) {
            if (!mem) return;
            for (size_t i = 0; i < sz; ++i) {
                Ls* p = mem[i];
                if (p && p != FULL) delete p;
            }
            delete [] mem;
            mem = NULL;
        }
        void _init(bool v, size_t sz = cbitsT) {
            mem = new Ls* [sz];
            if (v) {
                for (size_t i = 0; i < sz; ++i) {
                    mem[i] = FULL;
                }
                return;
            }
            memset(mem, 0, sizeof(Ls*) * sz);
        }
        void init(bool v) { _init(v); }
        bool full()  const { return fullcount  == cbitsT; }
        bool empty() const { return emptycount == cbitsT; }
        size_t first() const { return _first(); }
        size_t _first(size_t sz = cbitsT) const {
            for (size_t i = 0; i < sz; ++i) {
                Ls* p = mem[i];
                if (p) {
                    size_t base = i * sizeSub;
                    if (p == FULL) return base;
                    return base + p->first();
                }
            }
            return ~(size_t)0;
        }

        bool has(size_t i) const {
            Ls*& sub = mem[i / sizeSub];
            if (sub == NULL) return false;
            if (sub == FULL) return true;
            size_t in = i % sizeSub;
            return sub->has(in);
        }
        virtual bool _isLast(size_t index) const { return false; }
        void set(size_t i) {
            Ls*& sub = mem[i / sizeSub];
            if (sub == FULL) return;
            size_t in = i % sizeSub;
            if (sub == NULL) {
                sub = _create(0, _isLast(i / sizeSub));
                --emptycount;
            }
            sub->set(in);
            if (sub->full()) {
                delete sub;
                sub = FULL;
                ++fullcount;
            }
        }
        void unset(size_t i) {
            Ls*& sub = mem[i / sizeSub];
            if (sub == NULL) return;
            size_t in = i % sizeSub;
            if (sub == FULL) {
                sub = _create(1, _isLast(i / sizeSub));
                --fullcount;
            }
            sub->unset(in);
            if (sub->empty()) {
                delete sub;
                sub = NULL;
                ++emptycount;
            }
        }

        virtual size_t _maxSub() const { return 0; }
        virtual Ls* _create(bool v, bool last) {
            Ls* p = NULL;
            if (sizeSub == L0::ctotalT) p = last ? new L0h(_maxSub()) : new L0;
            else p = last ? new Lnh(_maxSub()) : new Ln(sizeSub / cbitsT);
            p->init(v);
            return p;
        }
    };

    struct Lnh : public Ln {
        size_t size;
        size_t memsz;
        Lnh(size_t max) : size(max), Ln(_subSize(max)) {
             memsz = (size + sizeSub - 1) / sizeSub;
             emptycount = memsz;
        }
        ~Lnh() { _free(memsz); }
        void init(bool v) { _init(v, memsz); }
        bool full() const  { return fullcount == memsz; }
        bool empty() const { return emptycount == memsz; }
        size_t first() const { return _first(memsz); }
        bool _isLast(size_t i) const { return i == (memsz - 1) && _maxSub(); }
        size_t _maxSub() const { return size % sizeSub; }
        static size_t _subSize(size_t sz) {
            size_t r = L0::ctotalT;
            while (sz / r) {
                if (r == sz) return r / cbitsT;
                r *= cbitsT; 
            }
            return r / cbitsT;
        }
    };

    Ls* impl;
    Bits(size_t max) : impl(NULL) {
        if (max < L0::ctotalT)       impl = new L0h(max);
        else if (max == L0::ctotalT) impl = new L0;
        else {
            size_t m = Lnh::_subSize(max);
            if (max == m * cbitsT)   impl = new Ln(m);
            else                     impl = new Lnh(max);
        }
        impl->init(0);
    }
    ~Bits() { delete impl; }
    void set(size_t i) { impl->set(i); }
    void unset(size_t i) { impl->unset(i); }
};

const Bits::Ln::LsP
Bits::Ln::FULL = (const Bits::Ln::LsP)1;

#endif /*bits_hh*/
