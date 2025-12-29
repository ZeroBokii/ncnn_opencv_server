# XMake 构建指南

## PC 开发

```bash
xmake           # 编译 (默认 amd)
xmake run       # 运行
```

## ARM 交叉编译

```bash
xmake f --arch=arm    # 切换到 ARM (自动使用 aarch64-linux-gnu-gcc)
xmake
```

## 切回 PC

```bash
xmake f --arch=amd
xmake
```

## 其他

```bash
xmake -r              # 重新编译
xmake clean           # 清理
xmake f -m release    # Release 模式
xmake f -m debug      # Debug 模式
```
