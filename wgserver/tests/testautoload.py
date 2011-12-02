import time
import autoreload
import logics.mod

if __name__ == "__main__":
    autoreload.reload_code()
    obj = logics.mod.C()
    obj.foo()
    time.sleep(30)
    autoreload.reload_code()
    obj = logics.mod.C()
    obj.foo()
    print "here"
