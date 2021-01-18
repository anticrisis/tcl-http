# http

A simple, fast HTTP server with Tcl handlers.

- Register a Tcl command for each HTTP method you want to accept.
- Set status code, headers and content-type on each response.
- Fast C++ server implementation based on Boost::Beast.
- Also includes simple synchronous HTTP client.

## Usage

```tcl
% package require act_http
% namespace import act::*
% http configure -host 127.0.0.1 -port 1234 -get {list 200 "hello, world" "text/plain"}
% http run
```

This is a pure C shared library extension, with no Tcl code. You can also load
the package by using `load` and a pathname to wherever you installed the
shared library.

## Configuration

Use the `http configure` command. With no arguments, it returns the current
configuration. These are the options:

- Host configuration
  - `-host`
  - `-port`
- HTTP handlers
  - `-head`
  - `-get`
  - `-post`
  - `-put`
  - `-delete`
  - `-options`
- Variables set on each callback
  - `-reqtargetvariable` : the target part of the request, e.g. "/home"
  - `-reqbodyvariable` : the body of the request
  - `-reqheadersvariable` : the headers of the request

## Running

Use `http run` to start the server. Because this implementation uses a
blocking read on socket I/O, you must use control-C to stop the server.

See the `examples` directory for examples.

## Building

After `git clone`, these four commands will build and install this extension.
Their meaning and function are described below.

```sh
$ act vcpkg setup
$ act vcpkg install
$ act cmake release
$ (sudo) act system install # optional
```

### C++

This extension requires C++17.

On Windows or Mac, install your vendor's C++ build tools. On Windows, I
recommend Microsoft Visual Studio Community Edition, which is free of charge for
open source projects. I believe it's also available on Mac. Or if you don't want
a complete IDE, you can search for and install the [Build Tools for Visual
Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019),
which are also free of charge.

On Unix/Linux, you probably already have build tools installed. If not, please
refer to other instructions.

### Initial setup

Use [vcpkg](https://github.com/microsoft/vcpkg) for painless cross-platform
C++ dependency management.

To aid in the initial setup of your development environment, the `act` script
and `act.bat` batch file are provided. `act` provides commands to download and
setup `vcpkg` on your machine, and to download and compile the C++
dependencies needed for this project.

`act` and `vcpkg` are compatible with Windows, Mac and Linux, provided you
have installed C++ build tools and they are available on your path.

For example, on Windows, start a Developer Command Prompt or use one of the
Visual Studio-provided batch files to initialize your environment for
development. You should ensure the Microsoft C++ compiler is present by trying
`cl -help`.

Ensure the parent directory of this repository is writable by you.

Then, in the root of this repository, run the `act vcpkg setup` or `act.bat
vcpkg setup` command. This will clone the `vcpkg` directory and perform the
initial bootstrap, which can take several minutes. Unfortunately, there is no
output during the compilation. If this makes you nervous, you should perform
the commands manually. A `-dry-run` option is provided by `act` to display the
commands without executing them.

### Installing dependencies

After completing the vcpkg setup with `act vcpkg setup`, you should run `act
vcpkg install`. This will download and compile the necessary dependencies,
which will take at least five minutes of compilation. (This project uses the
Boost Beast library, which is quite nice but not, shall we say, lightweight.
The resulting DLL is approximately 300KB.)

Again, you could perform this setup step manually if you prefer to see
progress output.

All dependencies are contained within the `vcpkg` directory. No files are
installed anywhere else on your system.

### Building this extension

Once `vcpkg` is set up, building and installing is as easy as:

```sh
$ act cmake release
$ (sudo) act system install
```

If you prefer not to install the built extension into your system library directory, you can refer
to the `install` directory created as part of the `act cmake release` step. It will contain the package directory you need to place on your `TCLLIBPATH` to use the extension.

### Verifying 

Test if you've successfully installed the package on your TCLLIBPATH:

```tcl
% package require act_http
%
```

## Tests

Basic tests are provided, which launch subprocesses to run the http server, and
use this extension's included http client to check for specific responses.

To run the tests, build the shared library, then ensure that the `act_http`
package directory containing `pkgIndex.tcl` is on your TCLLIBPATH, then:

```sh
$ cd test
$ tclsh all.tcl
```

Single test or files matching another glob regexp can be invoked like this, for
example to run the tests in url.tcl only:

```sh
$ tclsh all.tcl -file url.tcl
```

## Performance

This is an "on my machine" test of a simple "hello, world" app, which is of
course not representative of actual workloads. But it does serve to show the
minimal overhead added to a realistic workload by the http server itself.

```tcl
% package require act_http
% namespace import anticrisis:*
% http configure -host 127.0.0.1 -port 8080 -get {list 200 "hello, world" "text/plain"}
% http run
```

Result:

```sh
PS E:\> bombardier-windows-amd64.exe http://127.0.0.1:8080
Bombarding http://127.0.0.1:8080 for 10s using 125 connection(s)
[===================================================================================================================================] 10s
Done!
Statistics        Avg      Stdev        Max
  Reqs/sec    164035.38    9240.09  190945.90
  Latency      686.81us   380.20us   176.00ms
  HTTP codes:
    1xx - 0, 2xx - 1638899, 3xx - 0, 4xx - 0, 5xx - 0
    others - 0
  Throughput:    25.61MB/s
```

By contrast, a similar test using nodejs express:

```js
const express = require('express')
const app = express()
const port = 3000

app.get('/', (req, res) => {
  res.send('hello, world')
})

app.listen(port, () => {
  console.log(`listening at http:://localhost:${port}`)
})

```

```sh
PS E:\> bombardier-windows-amd64.exe http://127.0.0.1:3000 -l
Bombarding http://127.0.0.1:3000 for 10s using 125 connection(s)
[===================================================================================================================================] 10s
Done!
Statistics        Avg      Stdev        Max
  Reqs/sec      5553.01     773.61    6156.96
  Latency       22.49ms     2.64ms   140.00ms
  Latency Distribution
     50%    21.00ms
     75%    23.00ms
     90%    25.56ms
     95%    30.00ms
     99%    38.00ms
  HTTP codes:
    1xx - 0, 2xx - 55583, 3xx - 0, 4xx - 0, 5xx - 0
    others - 0
  Throughput:     1.59MB/s
```

And this example is using nodejs' built-in http module:

```js
const http = require('http')
const hostname = '127.0.0.1'
const port = 3000;

const server = http.createServer((req, res) => {
  res.statusCode = 200;
  res.setHeader('Content-Type', 'text/plain');
  res.end('hello, world')
})

server.listen(port, hostname, () => {
  console.log(`listening at http://${hostname}:${port}`)
})
```

```sh
PS E:\> bombardier-windows-amd64.exe http://127.0.0.1:3000 -l
Bombarding http://127.0.0.1:3000 for 10s using 125 connection(s)
[===================================================================================================================================] 10s
Done!
Statistics        Avg      Stdev        Max
  Reqs/sec     17463.05    2311.83   19928.00
  Latency        7.16ms     0.88ms    85.98ms
  Latency Distribution
     50%     7.00ms
     75%     7.01ms
     90%     8.00ms
     95%     8.02ms
     99%    14.98ms
  HTTP codes:
    1xx - 0, 2xx - 174529, 3xx - 0, 4xx - 0, 5xx - 0
    others - 0
  Throughput:     3.71MB/s
 ```

Of course, nodejs users will typically run multiple workers, each in its own
process, to increase utilisation of available CPU cores. This does add
additional complexity for some applications, for example when using a shared
database.

## License

BSD-3-Clause, just like (almost) Tcl.
