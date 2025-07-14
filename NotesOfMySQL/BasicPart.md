# Basic Part

## MySQL ����

### ���ݿ���ظ���

���ݿ⣺$DataBase$
���ݿ����ϵͳ��$DBMS$

����$SQL$
������ϵ�����ݿ�ı������
$Structured$ $Query$ $Language$

Learn about $MySQL$

��

#### ����ģ��

��ϵ�����ݿ⣺�ڹ�ϵģ�ͻ����ϣ�ʹ�ö����໥���ӵĶ�ά�����Ӷ��ɡ�

## SQL

����SQLͨ���﷨��SQL���ࡢDDL��DML��DQL��DCL��

### SQLͨ���﷨

�ֺŽ�β����������
ע�⣺**SQL ��䲻���ִ�Сд**

### SQL����

![alt text](image.png)

$Definition$��$Manipulation$��$Query$��$Control$

### $DDL$

$Data$ $Definition$ $Language$

��ѯ��
��ѯ�������ݿ⣺

```SQL
SHOW DATABSES
```

��ѯ��ǰ���ݿ⣺

```SQL
SELECT DATABASE();
```

������

```SQL
CREATE DATABASE [IF NOT EXISTS] ���ݿ��� [DEFAULT CHARSET �ַ���] [COLLATE �������];
```

ɾ����

```SQL
DROP DATABASE [IF EXISTS]���ݿ���;
```

ʹ�ã�

```SQL
USE ���ݿ���;
```

#### DDL-�����-��ѯ

��ѯ��ǰ���ݿ����б�

```SQL
SHOW TABLES;
```

��ѯ��ṹ��

```SQL
DESC ������;
```

��ѯָ����Ľ�����䣺

```SQL
SHOW CREATE TABLE ����;
```

#### DDL-�����-����

```SQL
CREATE TABLE ����(
    �ֶ�1 �ֶ�1����[COMMENT �ֶ�1ע��]
)[COMMENT ��ע��];
```

#### DDL-��������

����˵

```SQL
score double(4, 1)
```

��ʾ����Ϊ4��С����λ��Ϊ1

�������ͣ�DATE��TIME��YEAR��DATETIME

һ�����ӣ�

```sql
create table exp(
    id int,
    worknumber varchar(10),
    name varchar(10) comment 'name',
    gender char(1) comment '�Ա�',
    age tinyint unsigned comment '����',
    idcard char(18) comment '���֤��',
    entrydate date comment '��ְʱ��'
) comment 'information for worker';

```

һ�㶨����char���䳤��varchar

#### DDL-�����-�޸�

����ֶ�

```sql
ALTER TABLE ���� ADD �ֶ��� ���ͣ����ȣ�[comment ...]

```

�޸��������ͣ�

```sql
alter table ���� modify �ֶ��� ���������ͣ����ȣ�;
```

�޸��ֶ������ֶ����ͣ�

```sql
alter table ���� change ���ֶ��� ���ֶ��� ���ͣ����ȣ�[comment ע��][Լ��];
```

ɾ���ֶ�

```sql
alter table ���� drop �ֶ���;
```

�޸ı���

```sql
alter table ���� rename to �±���;
```

ɾ����

```sql
drop table [if exists] ����;
```

ɾ��ָ�����������´����ñ�

```sql
truncate table ����;
```

### $DML$

���������ݿ��б�����ݼ�¼������ɾ�Ĳ���

��ӣ�$Insert$
�޸ģ�$Update$
ɾ����$Delete$

#### $DML$-�������

��ָ���ֶ�������ݣ�

```sql
Insert Into �������ֶ���1���ֶ���2��Values��ֵ1��ֵ2��������;
```

��ȫ���ֶ�������ݣ�

```sql
Insert Into ���� Values ��ֵ1�� ֵ2�� ������;
```

����������ݣ�

```sql
// �����ʵ���Ǻ����Ӽ������ݴ�
```

#### $DML$-�޸�����

```sql
update ���� set �ֶ���1 = ֵ1���ֶ���2 = ֵ2�� ����[WHERE ����];
```

#### $DML$-ɾ������

```sql
Delete From ���� [Where ����]
```

�������$where$�������ͻ�����еĶ�����ɾ����

### $DQL$

$DQL$��$Data$ $Query$ $Language$

���Ǳ�**�㷺ʹ��**��һ�����

```sql
Select
    �ֶ��б�
From
    �����б�
Where
    �����б�
Group By
    �����ֶ��б�
Having
    ����������б�
Order By
    �����ֶ��б�
Limit
    ��ҳ����
```

#### $DQL$-������ѯ

��ѯ����ֶΣ�

```sql
Select �ֶ�1���ֶ�2���ֶ�3���� From ����;
Select * From ����; // All �ֶ�
```

���ñ�����

```sql
Select �ֶ�1 As ����1���ֶ�2 As ����2 From ����;
```

ȥ���ظ���¼��

```sql
Select Distinct �ֶ��б� From ����;
```

#### $DQL$-������ѯ

�﷨��

```sql
Select �ֶ��б� From ���� Where �����б�;
```

�ر�ע�⣺**$MySQL$�����ʹ����ֵ��ѯ��TRUE, FALSE, UNKNOWN**

ע���Ǹ�ģ��ƥ����﷨��ʽ����������������ӣ�

```sql
select * from emp where name like '___';
```

$m$���»���˵�����ֳ���$m$

```sql
select * from emp where idcard like '%1';
```

$\%$˵��ǰ�����λ������ν�����һ������1

#### $DQL$-�ۺϺ���

��һ��������Ϊһ�����壬����������㡣

�����ۺϺ�����$max$, $min$, $avg$, $sum$, $count$.

```sql
select ������ from ����;
```

$null$ֵ**������**�ۺϺ�������

#### $DQL$-�����ѯ

```sql
select �ֶ��б� From ���� [Where ����] Group By �����ֶ��� [Having ������������];
```

##### $where$��$Having$����

1. ִ��ʱ����ͬ��Where�Ƿ���ǰ���й��ˣ�������Where��������������飻��Having�Ƿ���֮��Խ�����й��ˣ�
2. �ж�������ͬ��Where���ܶԾۺϺ��������жϣ���Having���ԡ�

ע�⣺

a. ִ��˳��where > �ۺϺ��� > having
b. ����֮�󣬲�ѯ���ֶ�һ��Ϊ�ۺϺ����ͷ����ֶΣ���ѯ�����ֶ�û�����塣

#### $DQL$-�����ѯ

$ORDER$ $BY$

```sql
select �ֶ��б� from ���� order by �ֶ�1 ����ʽ1���ֶ�2, ����ʽ2;
```

����ʽ��
ASC������
DESC������

#### $DQL$-��ҳ��ѯ

$LIMIT$

�������Ū��������һҳһҳ�Ĺ���ҳ��

```sql
select �ֶ��б� from ���� limit ��ʼ����, ��ѯ��¼��;
```

ע��㣺

1. ��ʼ������0��ʼ��$$��ʼ���� = (��ѯҳ�� - 1) \times ÿҳ��ʾ��¼��$$
2. ��ҳ��ѯ�����ݿ��**����**����ͬ���ݿ�ʵ�ַ�ʽ��ͬ����$MySQL$����$LIMIT$
3. �����ѯ���ǵ�һҳ���ݣ���ʼ�������Ժ��ԣ�ֱ�Ӽ�дΪ$LIMIT$ $10$

#### $DQL$-ִ��˳��

ע������**��д**˳���**ִ��**˳��
ִ�д���from->where->group by->having->select->order by->limit

![alt text](image-1.png)

### $DCL$

��**���ݿ�������**��$Data$ $Control$ $Language$
�������ݿ��û����������ݿ�ķ���Ȩ�ޡ�

#### $DCL$-�����û�

��ѯ�û���

```sql
USE mysql; # ���mysql��ϵͳ���ݿ�
Select * From user;
```

�����û���

```sql
create user  '�û���'@'������' IDENTIFIED BY '����';
```

�޸��û����룺

```sql
alter user '�û���'@'������' IDENTIFIED WITH mysql_native_password BY '������';
```

ɾ���û���

```sql
DROP USER '�û���'@'������';
```

**������������$\%$ͨ��**

#### $DCL$-Ȩ�޿���

��ѯȨ�ޣ�

```sql
show grants for '�û���'@'������';
```

����Ȩ�ޣ�

```sql
GRANT Ȩ���б� ON ���ݿ���.���� TO '�û���'@'������';
```

����Ȩ�ޣ�

```sql
REVOKE Ȩ���б� ON ���ݿ���.���� FROM '�û���'@'������';
```

## Function/����
