temp:
	rm -rf tmp/hiredis
	mkdir -p tmp/hiredis
	cp -r README *.{cc,h,js*} wscript deps test tmp/hiredis
	cd tmp/hiredis && rm -rf deps/*/.git* deps/*/*.o deps/hiredis/libhiredis.*

package: temp
	cd tmp && tar -czvf hiredis.tgz hiredis

