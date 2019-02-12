#!/bin/bash

export PATH="../tools/wandiocat:$PATH"

OK=0
FAIL=""

do_test() {

        if $@; then
                OK=$[ OK + 1 ]
        else
                FAIL="$FAIL
$@"
        fi
}


do_read_test() {
        wandiocat $2 | md5sum | cut -d " " -f 1 > /tmp/wandiotest.md5

        diff -q /tmp/wandiotest.md5 /tmp/wandiobase.md5 > /dev/null

        if [ $? -ne 0 ]; then
                FAIL="$FAIL
reading $1 test file"
                echo "   fail"
        else
                OK=$[ OK + 1 ]
                echo "   pass"
        fi
}

do_write_test() {

        if [ $1 = "text" ]; then
                wandiocat -o /tmp/wandiowrite.out files/big.txt
        else
                wandiocat -z 1 -Z $1 -o /tmp/wandiowrite.out files/big.txt
        fi


        case $1 in
        text)
            TOOL="cat"
            ;;
        gzip)
            TOOL="gzip -d -c"
            ;;
        bzip2)
            TOOL="bzip2 -d -c"
            ;;
        lzma)
            TOOL="xz -d -c"
            ;;
        lz4)
            TOOL="lz4 -d -c"
            ;;
        zstd)
            TOOL="zstd -q -d -c"
            ;;
        lzo)
            TOOL="lzop -q -d -c"
            ;;
        *)
            echo "    fail (unrecognised format?)"
            FAIL="$FAIL
writing $1 test file"
            return
        esac

        $TOOL /tmp/wandiowrite.out | md5sum | cut -d " " -f 1 > /tmp/wandiotest2.md5

        if [ $1 != "lzo" ]; then
                wandiocat /tmp/wandiowrite.out | md5sum | cut -d " " -f 1 > /tmp/wandiotest.md5
                diff -q /tmp/wandiotest.md5 /tmp/wandiobase.md5 > /dev/null

                if [ $? -ne 0 ]; then
                        FAIL="$FAIL
        writing $1 test file"
                        echo "   fail (read with wandiocat)"
                        return
                fi
        fi


        diff -q /tmp/wandiotest2.md5 /tmp/wandiobase.md5 > /dev/null
        if [ $? -ne 0 ]; then
                FAIL="$FAIL
writing $1 test file"
                echo "   fail (read with standard tool)"
        else
                OK=$[ OK + 1 ]
                echo "   pass"
        fi
}

REQBINARIES=( gzip bzip2 xz lz4 zstd lzop )

for bin in ${REQBINARIES[*]}; do
        command -v $bin > /dev/null || echo "$bin not detected on system -- $bin tests may fail"
done

cat files/big.txt | md5sum | cut -d " " -f 1 > /tmp/wandiobase.md5

echo -n \* Reading text...
do_read_test text files/big.txt

echo -n \* Reading gzip...
do_read_test gzip files/big.txt.gz

echo -n \* Reading bzip2...
do_read_test bzip2 files/big.txt.bz2

echo -n \* Reading lzma...
do_read_test lzma files/big.txt.xz

echo -n \* Reading lz4...
do_read_test lz4 files/big.txt.lz4

echo -n \* Reading zstd...
do_read_test zstd files/big.txt.zst

echo -n \* Writing text...
do_write_test text

echo -n \* Writing gzip...
do_write_test gzip

echo -n \* Writing bzip2...
do_write_test bzip2

echo -n \* Writing lzma...
do_write_test lzma

echo -n \* Writing lz4...
do_write_test lz4

echo -n \* Writing zstd...
do_write_test zstd

echo -n \* Writing lzo...
do_write_test lzo

echo
echo "Tests passed: $OK"
echo "Tests failed: $FAIL"

if [ -z "$FAIL" ]; then
        exit 0
else
        exit 1
fi
