struct a1 {
  int x;
  virtual void f();
};
struct a2 {
  int y;
  virtual void f();
};
struct b : a1, a2 {
};

b* test(a2* v) {
	return dynamic_cast<b*>(v);
}
