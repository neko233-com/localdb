# LocalDB

**AI Agent 专用嵌入式数据库** — 替代 SQLite，为 Agent 场景优化。

## 为什么不用 SQLite？

AI Agent 使用 SQLite3 作为本地数据库时，常见的痛点：

| 问题 | SQLite | LocalDB |
|------|--------|---------|
| 对话日志追加慢 | 需要 B-tree 分页写入 | WAL 追加优化，批量写入 |
| JSON 存储不友好 | TEXT 字段，无原生支持 | 原生 JSON 文档 API |
| 异步 I/O 不支持 | 同步阻塞 | 异步友好 API |
| 大量临时状态清理麻烦 | 手动 DELETE | 内置 TTL 自动过期 |
| Agent 内存占用高 | 全量加载 | 懒加载 + LRU 缓存 |

## 核心特性

- **Document API** — 类 MongoDB 的 JSON 文档存储，key-value 风格
- **Batch Write** — 批量写入，高吞吐对话日志
- **TTL / Expiry** — 内置过期机制，适合 Agent 临时状态
- **WAL** — Write-Ahead Log，支持崩溃恢复
- **SQL 子集** — 支持基本的 CREATE/INSERT/SELECT/DELETE
- **多语言绑定** — C/C++/Rust/Go/Swift/Kotlin/Java/Python/TypeScript/C#/Dart/Zig

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## 快速开始

```c
#include "localdb.h"

int main() {
    localdb *db;
    localdb_open_memory(&db);

    // 创建集合
    localdb_collection_create(db, "memory");

    // 存储文档
    localdb_doc_put(db, "memory", "conv_001",
        "{\"role\":\"user\",\"content\":\"Hello\"}");

    // 读取文档
    char *json;
    localdb_doc_get(db, "memory", "conv_001", &json);
    printf("%s\n", json);
    localdb_free(json);

    // 批量写入
    localdb_batch *batch;
    localdb_batch_begin(db, &batch);
    localdb_batch_put(batch, "memory", "msg_002", "{\"role\":\"assistant\",\"content\":\"Hi!\"}");
    localdb_batch_put(batch, "memory", "msg_003", "{\"role\":\"user\",\"content\":\"Bye\"}");
    localdb_batch_commit(batch);

    // 设置 TTL（1小时后自动过期）
    localdb_doc_set_ttl(db, "memory", "conv_001", 3600);

    localdb_close(db);
}
```

## 语言绑定仓库

| 语言 | 仓库 |
|------|------|
| C (core) | [localdb](https://github.com/neko233-com/localdb) |
| C binding | [localdb-c](https://github.com/neko233-com/localdb-c) |
| C++ | [localdb-cpp](https://github.com/neko233-com/localdb-cpp) |
| Rust | [localdb-rust](https://github.com/neko233-com/localdb-rust) |
| Go | [localdb-go](https://github.com/neko233-com/localdb-go) |
| Swift | [localdb-swift](https://github.com/neko233-com/localdb-swift) |
| Kotlin | [localdb-kotlin](https://github.com/neko233-com/localdb-kotlin) |
| Java | [localdb-java](https://github.com/neko233-com/localdb-java) |
| Python | [localdb-python](https://github.com/neko233-com/localdb-python) |
| TypeScript (Web) | [localdb-typescript-web](https://github.com/neko233-com/localdb-typescript-web) |
| TypeScript (Server) | [localdb-typescript-server](https://github.com/neko233-com/localdb-typescript-server) |
| C# | [localdb-csharp](https://github.com/neko233-com/localdb-csharp) |
| Dart | [localdb-dart](https://github.com/neko233-com/localdb-dart) |
| Zig | [localdb-zig](https://github.com/neko233-com/localdb-zig) |

## License

MIT
