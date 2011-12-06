import sys
import database
db = database.Connection("localhost", "playerdb","root","123456")
print db
db.close()
#for user in db.query("SELECT * FROM account"):
#     print user
print db.execute('call login_user(@rv)')
print db.get('select @rv')


