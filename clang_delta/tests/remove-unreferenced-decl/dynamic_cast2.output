struct a {
  virtual void f();
};
struct b : a{
};

b& test(a& v) {
	return dynamic_cast<b&>(v);
}
