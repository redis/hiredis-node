all:
	node-gyp configure build

clean:
	node-gyp clean

temp:
	rm -rf tmp/hiredis
	mkdir -p tmp/hiredis
	cp -r README* COPYING *.js* binding.gyp src deps test tmp/hiredis
	cd tmp/hiredis && rm -rf deps/*/.git* deps/*/*.o deps/*/*.a

package: temp
	cd tmp && tar -czvf hiredis.tgz hiredis

check:
	npm test
