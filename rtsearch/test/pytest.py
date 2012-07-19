# -*- coding: utf-8 -*-
import redis 

r = redis.StrictRedis(host='127.0.0.1', port=6888)

while 1:
    item = '{"type":"user","id":10,"term":"周四海是一个伟大的领导人物，不一般啊","score":100,"data":"url://"}'
    r.set('data',item)
    item = '{"type":"user","id":11,"term":"周四海是一个伟大的领导人物","score":99,"data":"url://"}'
    r.set('data',item)

    #r.delete(item)
    #query = '{"keywords":"周四海伟大","types":[{"type":"user","start":0,"num":2,"sort":-1},{"type":"tags","start":0,"num":10,"sort":-1}]}'
    query = '{"keywords":"周四海伟大","types":[{"type":"user","start":0,"num":10,"sort":-1},{"type":"tags","start":0,"num":10,"sort":-1}]}'
    rv = r.get(query)
    print rv
