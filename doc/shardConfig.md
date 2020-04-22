## ChainSQL 分片配置文件说明

### 总体说明

- 配置文件去掉 ***[ips]*** ,***[ips_fixed]***, ***[validators]***，保留 ***[validation_seed]***
- 新增 ***[shard]***, ***[shard_file]*** ***[lookup_file]***, ***[sync_file]***, ***[committee_file]***


### 新增配置项

```

# role 可取值：lookup,sync,shard,committee，其中lookup与sync可以同时设置，’,’分隔
# shard_count : 分片的个数，默认为1；shard_index : 当前节点所在的分片的序号( 注 分片的序号从1开始)
[shard]
role = shard  
shard_count = 3
shard_index = 1

# shard节点的配置文件说明
[shard_file]
./shard.txt

# lookup节点的配置文件说明
[lookup_file]
./lookup.txt

# sync节点的配置文件说明
[sync_file]
./sync.txt

# committee节点的配置文件说明
[committee_file]
./committee.txt

```

### 新增配置文件

- lookup.txt

```
# lookup节点的ip信息
[lookup_ips]
127.0.0.1:5126
127.0.0.1:5127

# lookup节点的公钥信息
[lookup_public_keys]
n9Kj61YFbTrA4aeQ3BZs5TpwUdrc5FZNRfiMTWQVVhPuQ52hyYS2
n9LwSyouiaokQzRpQtDw14gSxhDgLP68W4mf8LkKzL9fXaL1oK64

```

- shard.txt

```
# shard节点的ips信息
# 分片的ips配置，第1行代表分片1的ips信息 以','号隔开；第2行代表分片2的ips信息 以','号隔开。
# 每一行代表每个分片的信息，分片的序号和行号保持一致。
[shard_ips]
127.0.0.1:5126,127.0.0.1:5127
127.0.0.1:5128,127.0.0.1:5129

# shard节点的validator信息
# 第1行代表分片1的validator信息, 以','号隔开；第2行代表分片2的ips信息 以','号隔开。
# 每一行代表每个分片的信息，分片的序号和行号保持一致。
[shard_validators]
n9Kj61YFbTrA4aeQ3BZs5TpwUdrc5FZNRfiMTWQVVhPuQ52hyYS2,n9LwSyouiaokQzRpQtDw14gSxhDgLP68W4mf8LkKzL9fXaL1oK64
n9Kj61YFbTrA4aeQ3BZs5TpwUdrc5FZNRfiMTWQVVhPuQ52hyYS2,n9LwSyouiaokQzRpQtDw14gSxhDgLP68W4mf8LkKzL9fXaL1oK64

```

- committee.txt

```
# committee节点的ips信息
[committee_ips]
127.0.0.1:5126
127.0.0.1:5127

# committee节点的validator信息
[committee_validators]
n9Kj61YFbTrA4aeQ3BZs5TpwUdrc5FZNRfiMTWQVVhPuQ52hyYS2
n9LwSyouiaokQzRpQtDw14gSxhDgLP68W4mf8LkKzL9fXaL1oK64
```

- sync_file.txt 

```
# sync节点的ips信息
[sync_ips]
127.0.0.1:5126
127.0.0.1:5127

```

### 配置项配置规则

1 如果角色是 lookup      , 必须配置  ***[shard_file]***
2 如果角色是 shard       , 必须配置  ***[lookup_file]*** ***[shard_file]*** ***[committee_file]*** ***[sync_file]***
3 如果角色是 committee   , 必须配置  ***[lookup_file]*** ***[shard_file]*** ***[committee_file]*** ***[sync_file]***
4 角色可以是 lookup shard committee  sync 的组合。