template <class> struct a {
  void b();
  void b() const;
  void c(a p1) { b()(p1); }
};
