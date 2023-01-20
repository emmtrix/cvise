struct a {
  void operator=(a);
};
struct b {};
struct s : b, a {
} c, d;
void e() { c = d; }
