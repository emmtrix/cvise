
template<class T>
struct Test {
	void f1() {
	}
	
	void f2() {
	}
};

void f3() {
	Test<int>().f1();
}