template <int a> struct b { static const int c = a; };
template <bool d> using e = b<d>;
struct f : b<e<__is_integral(int)>::c> {};
f F;
