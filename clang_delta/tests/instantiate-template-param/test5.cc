template <class a> a b();
template <class a> struct c {
  template <class f> c(f) : d(b<f>()) {}
  a d;
};
template <class e> struct i : c<e> {
  template <class g, class h> i(g, h) : c<e>(g()) {}
};
struct j {
  i<int *> k;
  j();
};
j::j() : k(nullptr, int()) {}
