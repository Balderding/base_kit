// msvc_stl.h: the few pieces of Microsoft's C++ standard library (MSVC STL, as in VS 2022) whose RESULTS are not
// fixed by the C++ standard, written out so GEOS built with clang + libstdc++ gives the very same answers as the
// MSVC-built geos.dll in shapely's wheel. patch_geos.py makes GEOS use these instead of the std ones.
//
//   geos_msvc::sort          std::sort is not stable: which of two equal elements comes first depends on the
//                            algorithm. MSVC: introsort, insertion sort at <= 32 elements, pivot by median of three
//                            (Tukey's ninther above 40), three-way "fat" partition, heap sort past 1.5 log2(N) splits.
//   geos_msvc::hash_double   std::hash<double>: MSVC = FNV-1a over the 8 bytes (with -0.0 hashed as 0.0);
//   geos_msvc::hash_size     std::hash<size_t>: FNV-1a over the 8 bytes (libstdc++: the value itself).
//   geos_msvc::unordered_map the iteration order of std::unordered_map: MSVC keeps every element in one list and
//                            each bucket as a [lo, hi] run in it, power-of-two buckets (8 at first, x8 while < 512,
//                            then x2), max load factor 1. Only the members GEOS uses are here.
// (std::priority_queue / push_heap / pop_heap / make_heap: libstdc++ and MSVC do the same moves; not needed.)
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <utility>
#include <vector>
#include <iterator>
#include <functional>

namespace geos_msvc {

// ---------------- sort (MSVC <algorithm>: _Sort_unchecked and helpers) ----------------
namespace detail {
using std::swap;

template <class It, class Pr>
inline void insertion_sort(It first, It last, Pr pred) {        // _Insertion_sort_unchecked
    if (first == last) return;
    for (It mid = first; ++mid != last;) {
        It hole = mid;
        auto val = std::move(*mid);
        if (pred(val, *first)) {                                  // new earliest element: move to front
            std::move_backward(first, mid, ++hole);
            *first = std::move(val);
        } else {
            for (It prev = hole; pred(val, *--prev); hole = prev) *hole = std::move(*prev);
            *hole = std::move(val);
        }
    }
}

template <class It, class Pr>
inline void med3(It first, It mid, It last, Pr pred) {           // _Med3_unchecked
    using std::iter_swap;
    if (pred(*mid, *first)) iter_swap(mid, first);
    if (pred(*last, *mid)) {
        iter_swap(last, mid);
        if (pred(*mid, *first)) iter_swap(mid, first);
    }
}

template <class It, class Pr>
inline void guess_median(It first, It mid, It last, Pr pred) {   // _Guess_median_unchecked (last is inclusive)
    using D = typename std::iterator_traits<It>::difference_type;
    const D count = last - first;
    if (40 < count) {                                             // Tukey's ninther
        const D step = (count + 1) >> 3;
        const D two_step = step << 1;
        detail::med3(first, first + step, first + two_step, pred);
        detail::med3(mid - step, mid, mid + step, pred);
        detail::med3(last - two_step, last - step, last, pred);
        detail::med3(first + step, mid, last - step, pred);
    } else {
        detail::med3(first, mid, last, pred);
    }
}

template <class It, class Pr>
inline std::pair<It, It> partition_by_median_guess(It first, It last, Pr pred) {   // _Partition_by_median_guess_unchecked
    using std::iter_swap;
    It mid = first + ((last - first) >> 1);
    detail::guess_median(first, mid, last - 1, pred);
    It pfirst = mid;
    It plast = pfirst + 1;

    while (first < pfirst && !pred(*(pfirst - 1), *pfirst) && !pred(*pfirst, *(pfirst - 1))) --pfirst;
    while (plast < last && !pred(*plast, *pfirst) && !pred(*pfirst, *plast)) ++plast;

    It gfirst = plast;
    It glast = pfirst;

    for (;;) {                                                    // partition
        for (; gfirst < last; ++gfirst) {
            if (pred(*pfirst, *gfirst)) {
                continue;
            } else if (pred(*gfirst, *pfirst)) {
                break;
            } else if (plast != gfirst) {
                iter_swap(plast, gfirst);
                ++plast;
            } else {
                ++plast;
            }
        }
        for (; first < glast; --glast) {
            if (pred(*(glast - 1), *pfirst)) {
                continue;
            } else if (pred(*pfirst, *(glast - 1))) {
                break;
            } else if (--pfirst != glast - 1) {
                iter_swap(pfirst, glast - 1);
            }
        }
        if (glast == first && gfirst == last) return std::pair<It, It>(pfirst, plast);

        if (glast == first) {                                     // no room at bottom, rotate pivot upward
            if (plast != gfirst) iter_swap(pfirst, plast);
            ++plast;
            iter_swap(pfirst, gfirst);
            ++pfirst;
            ++gfirst;
        } else if (gfirst == last) {                              // no room at top, rotate pivot downward
            if (--glast != --pfirst) iter_swap(glast, pfirst);
            iter_swap(pfirst, --plast);
        } else {
            iter_swap(gfirst, --glast);
            ++gfirst;
        }
    }
}

// heap: _Push_heap_by_index, _Pop_heap_hole_by_index, _Make_heap_unchecked, _Sort_heap_unchecked
template <class It, class D, class T, class Pr>
inline void push_heap_by_index(It first, D hole, D top, T &&val, Pr pred) {
    for (D idx = (hole - 1) >> 1; top < hole && pred(*(first + idx), val); idx = (hole - 1) >> 1) {
        *(first + hole) = std::move(*(first + idx));
        hole = idx;
    }
    *(first + hole) = std::forward<T>(val);
}
template <class It, class D, class T, class Pr>
inline void pop_heap_hole_by_index(It first, D hole, D bottom, T &&val, Pr pred) {
    const D top = hole;
    D idx = hole;
    const D max_non_leaf = (bottom - 1) >> 1;
    while (idx < max_non_leaf) {
        idx = 2 * idx + 2;
        if (pred(*(first + idx), *(first + (idx - 1)))) --idx;
        *(first + hole) = std::move(*(first + idx));
        hole = idx;
    }
    if (idx == max_non_leaf && bottom % 2 == 0) {
        *(first + hole) = std::move(*(first + (bottom - 1)));
        hole = bottom - 1;
    }
    detail::push_heap_by_index(first, hole, top, std::forward<T>(val), pred);
}
template <class It, class Pr>
inline void make_heap_msvc(It first, It last, Pr pred) {
    using D = typename std::iterator_traits<It>::difference_type;
    D bottom = last - first;
    for (D hole = bottom >> 1; hole > 0;) {
        --hole;
        auto val = std::move(*(first + hole));
        detail::pop_heap_hole_by_index(first, hole, bottom, std::move(val), pred);
    }
}
template <class It, class Pr>
inline void sort_heap_msvc(It first, It last, Pr pred) {
    using D = typename std::iterator_traits<It>::difference_type;
    for (; last - first >= 2; --last) {
        It l = last - 1;
        auto val = std::move(*l);
        *l = std::move(*first);
        detail::pop_heap_hole_by_index(first, D(0), D(l - first), std::move(val), pred);
    }
}

template <class It, class D, class Pr>
inline void sort_unchecked(It first, It last, D ideal, Pr pred) {
    for (;;) {
        if (last - first <= 32) {                                 // _ISORT_MAX
            detail::insertion_sort(first, last, pred);
            return;
        }
        if (ideal <= 0) {                                         // heap sort if too many divisions
            detail::make_heap_msvc(first, last, pred);
            detail::sort_heap_msvc(first, last, pred);
            return;
        }
        auto m = detail::partition_by_median_guess(first, last, pred);
        ideal = (ideal >> 1) + (ideal >> 2);                      // allow 1.5 log2(N) divisions
        if (m.first - first < last - m.second) {
            detail::sort_unchecked(first, m.first, ideal, pred);
            first = m.second;
        } else {
            detail::sort_unchecked(m.second, last, ideal, pred);
            last = m.first;
        }
    }
}
} // namespace detail

template <class It, class Pr>
inline void sort(It first, It last, Pr pred) {
    detail::sort_unchecked(first, last, last - first, pred);
}
template <class It>
inline void sort(It first, It last) {
    geos_msvc::sort(first, last, std::less<>{});
}

// ---------------- hashes (MSVC <type_traits>: _Fnv1a_append_bytes) ----------------
inline std::size_t fnv1a(const unsigned char *p, std::size_t n) {
    std::size_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < n; ++i) { h ^= static_cast<std::size_t>(p[i]); h *= 1099511628211ULL; }
    return h;
}
struct hash_double {
    std::size_t operator()(double v) const noexcept {
        double k = (v == 0.0) ? 0.0 : v;                          // -0.0 and 0.0 hash alike
        unsigned char b[sizeof k]; std::memcpy(b, &k, sizeof k); return fnv1a(b, sizeof k);
    }
};
struct hash_size {
    std::size_t operator()(std::size_t v) const noexcept {
        unsigned char b[sizeof v]; std::memcpy(b, &v, sizeof v); return fnv1a(b, sizeof v);
    }
};

// ---------------- unordered_map (MSVC <xhash>, unique keys) ----------------
template <class K, class V, class H, class Eq = std::equal_to<K>>
class unordered_map {
public:
    using value_type = std::pair<const K, V>;
    using size_type = std::size_t;
private:
    struct Node { Node *prev, *next; value_type val; template <class... A> Node(A &&...a) : prev(nullptr), next(nullptr), val(std::forward<A>(a)...) {} };
    struct Head { Node *prev, *next; };
    Head head_;                                // the list's sentinel (only prev/next used)
    std::vector<Node *> vec_;                  // 2 per bucket: lo, hi (inclusive); end() when empty
    size_type mask_, nb_, size_;
    H hf_; Eq eq_;
    Node *end_node() const { return reinterpret_cast<Node *>(const_cast<Head *>(&head_)); }
    struct Found { Node *insert_before; Node *dup; };
    Found find_last(const K &k, size_type hv) const {           // _Find_last
        const size_type b = hv & mask_;
        Node *where = vec_[(b << 1) + 1];
        Node *e = end_node();
        if (where == e) return {e, nullptr};
        Node *lo = vec_[b << 1];
        for (;;) {
            if (eq_(k, where->val.first)) return {where->next, where};
            if (where == lo) return {where, nullptr};
            where = where->prev;
        }
    }
    Node *insert_before(size_type hv, Node *before, Node *n) {  // _Insert_new_node_before
        Node *after = before->prev;
        ++size_;
        n->next = before; n->prev = after;
        after->next = n; before->prev = n;
        Node *e = end_node();
        const size_type b = hv & mask_;
        Node *&lo = vec_[b << 1]; Node *&hi = vec_[(b << 1) + 1];
        if (lo == e) { lo = n; hi = n; }
        else if (lo == before) lo = n;
        else if (hi == after) hi = n;
        return n;
    }
    static size_type ceil_log2(size_type v) { size_type l = 0; while ((size_type(1) << l) < v) ++l; return l; }
    size_type min_load_buckets(size_type n) const { return static_cast<size_type>(std::ceil(static_cast<float>(n) / 1.0f)); }
    void forced_rehash(size_type buckets) {                     // _Forced_rehash
        const size_type lg = ceil_log2(buckets);
        nb_ = size_type(1) << lg; mask_ = nb_ - 1;
        Node *e = end_node();
        vec_.assign(nb_ << 1, e);
        Node *ins = head_.next;
        while (ins != e) {
            Node *next_ins = ins->next;
            const size_type b = hf_(ins->val.first) & mask_;
            Node *&lo = vec_[b << 1]; Node *&hi = vec_[(b << 1) + 1];
            if (lo == e) { lo = ins; hi = ins; ins = next_ins; continue; }
            // unique keys: no equivalent key in the bucket, so MSVC splices ins to the BEGINNING of the bucket
            // ("There are no equivalent keys in the bucket, so insert it at the beginning."): every rehash
            // reverses the order inside a bucket.
            Node *before = lo;
            ins->prev->next = ins->next; ins->next->prev = ins->prev;         // unlink
            Node *after = before->prev;
            ins->prev = after; ins->next = before; after->next = ins; before->prev = ins;
            lo = ins;
            (void)hi;
            ins = next_ins;
        }
    }
    bool rehash_required_1() const { return 1.0f < static_cast<float>(size_ + 1) / static_cast<float>(nb_); }
    size_type desired_grow(size_type for_size) const {
        const size_type old = nb_;
        size_type req = min_load_buckets(for_size); if (req < old) req = old;
        if (req > old) { if (old < 512) return old * 8; return old * 2; }
        return req;
    }
public:
    template <bool C> class iter {
        friend class unordered_map;
        Node *p_;
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = typename unordered_map::value_type;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<C, const value_type &, value_type &>;
        using pointer = std::conditional_t<C, const value_type *, value_type *>;
        iter(Node *p = nullptr) : p_(p) {}
        template <bool C2, class = std::enable_if_t<C && !C2>> iter(const iter<C2> &o) : p_(o.p_) {}
        reference operator*() const { return p_->val; }
        pointer operator->() const { return &p_->val; }
        iter &operator++() { p_ = p_->next; return *this; }
        iter operator++(int) { iter t = *this; p_ = p_->next; return t; }
        iter &operator--() { p_ = p_->prev; return *this; }
        friend bool operator==(const iter &a, const iter &b) { return a.p_ == b.p_; }
        friend bool operator!=(const iter &a, const iter &b) { return a.p_ != b.p_; }
    };
    using iterator = iter<false>;
    using const_iterator = iter<true>;

    unordered_map() : mask_(7), nb_(8), size_(0) {
        head_.prev = head_.next = end_node();
        vec_.assign(16, end_node());
    }
    unordered_map(const unordered_map &) = delete;
    unordered_map &operator=(const unordered_map &) = delete;
    ~unordered_map() { clear(); }

    size_type size() const { return size_; }
    bool empty() const { return size_ == 0; }
    size_type bucket_count() const { return nb_; }
    iterator begin() { return iterator(head_.next); }
    iterator end() { return iterator(end_node()); }
    const_iterator begin() const { return const_iterator(head_.next); }
    const_iterator end() const { return const_iterator(end_node()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }

    iterator find(const K &k) { Found f = find_last(k, hf_(k)); return f.dup ? iterator(f.dup) : end(); }
    const_iterator find(const K &k) const { Found f = find_last(k, hf_(k)); return f.dup ? const_iterator(f.dup) : end(); }
    size_type count(const K &k) const { return find_last(k, hf_(k)).dup ? 1 : 0; }

    template <class KK, class... A>
    std::pair<iterator, bool> try_emplace(KK &&k, A &&...a) {    // _Try_emplace
        const size_type hv = hf_(k);
        Found f = find_last(k, hv);
        if (f.dup) return {iterator(f.dup), false};
        Node *n = new Node(std::piecewise_construct, std::forward_as_tuple(std::forward<KK>(k)), std::forward_as_tuple(std::forward<A>(a)...));
        if (rehash_required_1()) { forced_rehash(desired_grow(size_ + 1)); f = find_last(n->val.first, hv); }
        return {iterator(insert_before(hv, f.insert_before, n)), true};
    }
    template <class... A>
    std::pair<iterator, bool> emplace(A &&...a) {                // emplace: node first, then the lookup
        Node *n = new Node(std::forward<A>(a)...);
        const K &k = n->val.first;
        const size_type hv = hf_(k);
        Found f = find_last(k, hv);
        if (f.dup) { delete n; return {iterator(f.dup), false}; }
        if (rehash_required_1()) { forced_rehash(desired_grow(size_ + 1)); f = find_last(k, hv); }
        return {iterator(insert_before(hv, f.insert_before, n)), true};
    }
    std::pair<iterator, bool> insert(const value_type &v) { return emplace(v); }
    V &operator[](const K &k) { return try_emplace(k).first->second; }

    void clear() {
        Node *e = end_node(), *p = head_.next;
        while (p != e) { Node *q = p->next; delete p; p = q; }
        head_.prev = head_.next = e; size_ = 0;
        vec_.assign(nb_ << 1, e);
    }
};

} // namespace geos_msvc
