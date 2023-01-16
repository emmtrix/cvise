template <class> struct e { template <int> using f = int; };
template <int g, int h>
using i = typename __make_integer_seq<e, int, g>::template f<h>;
template <int g, int h = 0> struct j { typedef i<g, h> aa; };
int k, m, n;
struct o {
  template <class... l, class... ab>
  o(int s, int t, int ac)
      : o(s, t, ac, typename j<sizeof...(l)>::aa(),
          typename j<sizeof...(ab)>::aa()) {}
  o(int, int, int, int, int);
};
template <class> struct ad;
template <class b> struct ad<b *> {
  typedef b p;
  template <class q> using ae = q *;
};
template <class, class> struct u {};
template <class b, class> struct af { b r; };
template <class ag> struct v {
  using ah = ag;
  typedef typename ah::ai ai;
  template <class b, class... aj> static void ak(ah s, b t, aj... ac) {
    s.ak(t, ac...);
  }
};
template <class b> struct w {
  typedef b *ai;
  template <class q, class... aj> void ak(q *, aj... t) { q(t...); }
};
typedef v<w<af<u<int, int>, int>>>::ai ai;
struct x {
  x(w<af<u<int, int>, u<int, int> *>>);
};
struct y {
  y(ai, x);
  ai operator->();
};
template <class> struct al;
template <class am, class b> struct al<u<am, b>> { static o *an(u<am, b>); };
template <class ao, class = typename ad<ao>::p> struct z;
template <class ao, class b, class ap> struct z<ao, af<b, ap>> : al<b> {};
struct {
  template <class... aj> void aq(void(), aj... t) {
    w<af<u<int, int>, u<int, int> *>> a;
    y b(0, a);
    o c = *z<ad<u<int, int> *>::ae<af<u<int, int>, int>>>::an(b->r);
    v<w<af<u<int, int>, u<int, int> *>>>::ak(a, &c, t...);
  }
} d;
void ar() { d.aq(ar, k, m, n); }
