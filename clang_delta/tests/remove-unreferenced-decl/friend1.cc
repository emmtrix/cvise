template <class> struct a { friend a operator+(a, char); };
char b;
void d() { a<char> c = c + b; }
