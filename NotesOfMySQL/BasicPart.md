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

ע���Ǹ�ģ��ƥ����﷨��ʽ����������������ӣ�

```sql
select * from emp where name like '___';
```

$m$���»���˵�����ֳ���$m$

```sql
select * from emp where idcard like '%1';
```

$\%$˵��ǰ�����λ������ν�����һ������1
