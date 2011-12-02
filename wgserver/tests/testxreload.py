import sys
import os
import tempfile
from xreload import xreload

SAMPLE_CODE = """
class C:
    def foo(self):
        print(0)
	return 0
    @classmethod
    def bar(cls):
        print(0, 0)
    @staticmethod
    def stomp():
        print (0, 0, 0)
"""

tempdir = None
save_path = None


def setUp(unused=None):
    global tempdir, save_path
    tempdir = tempfile.mkdtemp()
    save_path = list(sys.path)
    sys.path.append(tempdir)

def make_mod(name="x", repl=None, subst=None):
    if not tempdir:
        setUp()
        assert tempdir
    fn = os.path.join(tempdir, name + ".py")
    f = open(fn, "w")
    sample = SAMPLE_CODE
    if repl is not None and subst is not None:
        sample = sample.replace(repl, subst)
    try:
        f.write(sample)
    finally:
        f.close()

if __name__ == "__main__":
    make_mod()
    import x

    for name, module in sys.modules.items():
	if module is not None:
	    f = getattr(module, '__file__', None)
	    if f is None: 
		continue
	    print module

    C = x.C
    Cfoo = C.foo
    Cbar = C.bar
    Cstomp = C.stomp

    b = C()
    bfoo = b.foo  
    b.foo() 
    bfoo() 
    Cfoo(b)
    Cbar()
    Cstomp()

    #modify mod and reload
    count = 0
    while count < 1:
	count += 1
	make_mod(repl="0", subst=str(count)) 
	xreload(x) 

	C = x.C
	Cfoo = C.foo
	Cbar = C.bar
	Cstomp = C.stomp
        b = C()
        bfoo = b.foo  
	b.foo() 
	bfoo() 
	Cfoo(b)
	Cbar()
	Cstomp()

