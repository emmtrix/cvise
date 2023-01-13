template <class a, a> struct b;
struct c {
  template <class> static void e();
};
template <class a> struct d : b<bool, __is_same(decltype(c::e<a>), int)> {};
