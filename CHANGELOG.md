### 0.3.0 (2015-04-03)

* Update to latest hiredis including basic windows support
* Refactore to use only the parser from hiredis source.

### 0.2.0 (2015-02-08)

* Update to use new hiredis 0.12
* Update nan to latest version to get io.js support for free (thanks @jonathanong)
* Bufferify writeCommand, to support unicode and non-string arguments and make it even faster (thanks @stephank)
* Remove support for Node 0.6
