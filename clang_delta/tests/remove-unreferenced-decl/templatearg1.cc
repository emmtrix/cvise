template <typename> struct a;
template <template <class> class = a> struct b { static const bool c = true; };
int d[b<>::c];
