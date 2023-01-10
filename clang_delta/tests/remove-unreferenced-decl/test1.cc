

struct Test {
	void f1() {
	}
	
	void f2() {
	}
};

typedef Test Test2;
typedef Test Test3;


void f3() {
	Test2().f1();
}
