use playerdb;
DELIMITER //
DROP PROCEDURE IF EXISTS create_account //
CREATE  PROCEDURE create_account(in in_uuid bigint unsigned,
    in in_uname char(64),
    in in_email char(64),
    in in_pwd char(32),
    out out_ret int)
begin
    set out_ret = 1;

    select count(*) into  @a from account where name = in_uname;

    if (0 = @a)  then
	insert into account(accountid,name,mail,password)
	values(in_uuid,in_uname,in_email,in_pwd,in_nickname);
	set out_ret = 0;
    else
	    set out_ret = 1;
    end if;
end //

