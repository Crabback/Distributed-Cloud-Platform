# PennCloud - Team T02

Welcome to our PennCloud implementation! This project uses Bazel for dependency management and build - see
https://bazel.build/install/os-x for installation instructions on your OS.

To build PennCloud, run the following:

```shell
bazel build ...
```

(The "..." is literal, not a placeholder.)

The first build will take quite some time (10-15min) as it will download and compile protobuf and gRPC.

## Build Debugging

### Unsupported C++ standard

This project uses the C++17 standard.

You may need to set up a `.bazelrc` file in the project root with the following contents (if one is not already
present):

```text
build --action_env=BAZEL_CXXOPTS="-std=c++17"
```

### OpenSSL not found

This project uses OpenSSL for its hash functions, and expects OpenSSL headers to be located
at `/usr/local/opt/openssl/include`. On some systems (e.g. the 505 VM) it may be installed elsewhere; you can symlink it
as follows:

```shell
sudo mkdir -p /usr/local/opt/openssl
sudo ln -s /usr/include/openssl /usr/local/opt/openssl/include
```

### ARM CPU Architecture (M1 Macs)

If using a M1 Mac, you will not be able to install Bazel from apt, as the apt package only includes amd64 architecture.
Instead, use [Bazelisk](https://github.com/bazelbuild/bazelisk) by downloading the latest release binary from the 
Releases page (`bazelisk-darwin-arm64` for macOS, `bazelisk-linux-arm64` for class VM) and using that as your `bazel`
binary.

## KV Server

The KV store is divided into 2 components: the master node, which supervises each of the storage nodes and handles
sending the storage node view to clients, and the storage nodes, which partition and replicate the data.

### Replica Definition

To define the nodes, create a file in the following format:

```text
master IP + port
partition node IP + port,node IP + port,...
...
```

For example, to run 3 data partitions with 3 storage nodes per partition:

```text
127.0.0.1:5000
127.0.0.1:6000,127.0.0.1:6001,127.0.0.1:6002
127.0.0.1:7000,127.0.0.1:7001,127.0.0.1:7002
127.0.0.1:8000,127.0.0.1:8001,127.0.0.1:8002
```

### Running the Server

To run the server, run:

```shell
bazel-bin/src/kvmaster <path to config>
bazel-bin/src/kv <db file path> <log file path> <path to config> <partition no, 1-indexed> <node index, 0-indexed>
```

For example, with the config defined above, to run the node with address `127.0.0.1:7001`, you would
run something like `bazel-bin/src/kv data/db.bin data/log.bin config.txt 2 1`.

### Partition Logic

The KV server partitions the data across the defined partitions by the row key. Specifically, if there are *N*
partitions defined, the data is partitioned as such:

Nodes in partition *i* are responsible for all rows with key *r* such that `lsb(md5(r)) % N == i` (assuming
little-endian).

### Primary Logic

When a KV node starts up, it calls the master to figure out who the primary of their partition is.

When a KV node gets a request for a write:

- if it does not know who the primary of the partition is, it returns an error
- if it is the primary, it multicasts the write to its partition and applies it
- if it is not the primary, it forwards it to the primary

### Logging and Checkpointing

The KV server logs all write (i.e. PUT, CPUT, DELETE) requests to a local append-only log, which it uses to replay
operations after a crash which does not allow for a complete checkpoint to be saved.

This log is checkpointed every 1 minute.

The log consists of a sequence of either of the following messages. We include the number of bytes of each argument
in the header as all fields are allowed to contain arbitrary character sequences, including newlines and null bytes.

```text
PUT <SP> rowbytes <SP> colbytes <SP> databytes <LF>
row <LF>
col <LF>
(*databytes* bytes of data follow) <LF>
```

```text
DELETE <SP> rowbytes <SP> colbytes <LF>
row <LF>
col <LF>
```

The checkpoint consists of a similar format: a sequence of the following message, in an arbitrary order.

```text
DATA <SP> rowbytes <SP> colbytes <SP> databytes <LF>
row <LF>
col <LF>
(*databytes* bytes of data follow) <LF>
```

### Tablet Sync

If a KV node goes down while other KV nodes in its partition (its "peers") are still serving requests, it will
communicate with its peers to bring its local copy of the partition up to date with the rest of the peers.

Writes are locked (by locking writes to the log file) from the peer serving the sync until the data transfer is
complete; reads are still served during this time.

## Test Scripts and Miscellany

Compile with:

```shell
bazel build //src/test:all
```

### KV Startup

```shell
bazel-bin/src/kvmaster kvconfig_min.txt
bazel-bin/src/kv data/db1.bin data/log1.bin kvconfig_min.txt 1 0
bazel-bin/src/kv data/db1_1.bin data/log1_1.bin kvconfig_min.txt 1 1
```

### KV Test

Each time the script is run, connects to a KV master at `127.0.0.1:5000` and increases the value of `(test, foo)` by 1.
If the key is undefined sets it to 1.
Prints the value of the key before it was updated. Useful for testing KV impl.

```shell
bazel-bin/src/test/kvtest
```

### KV Shell

```shell
bazel-bin/src/test/kvshell
```

### Webmail & Storage Service Test
Start the kvmaster and kv nodes first
```shell
bazel-bin/src/fe_servers/frontend_server 8080
bazel-bin/src/webmail/webmail -v
```

### Admin Server Setup

```shell
bazel-bin/src/fe_servers/admin_server
```

### Run Everything

:rocket_ship:

```shell
bazel-bin/src/kvmaster kvconfig_full.txt &
bazel-bin/src/kv data/dbfull_1_0.bin data/logfull_1_0.bin kvconfig_full.txt 1 0 &
bazel-bin/src/kv data/dbfull_1_1.bin data/logfull_1_1.bin kvconfig_full.txt 1 1 &
bazel-bin/src/kv data/dbfull_1_2.bin data/logfull_1_2.bin kvconfig_full.txt 1 2 &
bazel-bin/src/kv data/dbfull_2_0.bin data/logfull_2_0.bin kvconfig_full.txt 2 0 &
bazel-bin/src/kv data/dbfull_2_1.bin data/logfull_2_1.bin kvconfig_full.txt 2 1 &
bazel-bin/src/kv data/dbfull_2_2.bin data/logfull_2_2.bin kvconfig_full.txt 2 2 &
bazel-bin/src/kv data/dbfull_3_0.bin data/logfull_3_0.bin kvconfig_full.txt 3 0 &
bazel-bin/src/kv data/dbfull_3_1.bin data/logfull_3_1.bin kvconfig_full.txt 3 1 &
bazel-bin/src/kv data/dbfull_3_2.bin data/logfull_3_2.bin kvconfig_full.txt 3 2 &
```

Wait for the KV server to come online, then:

```shell
bazel-bin/src/webmail/webmail -v &
bazel-bin/src/fe_servers/frontend_server 8080 &
bazel-bin/src/fe_servers/frontend_server 8081 &
bazel-bin/src/fe_servers/frontend_server 8082 &
bazel-bin/src/fe_lb/loadbalancer &
bazel-bin/src/fe_servers/admin_server kvconfig_full.txt &
```

To use Penncloud, open `127.0.0.1:809090` in your browser.

To kill individual nodes, `ps` and `kill`!
To shutdown, use `pkill`.

ps -u 