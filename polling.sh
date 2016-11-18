#!/bin/sh
dir=`dirname $0`
pwd=`cd $dir; pwd -P`
cd $pwd

FPIPE=./files.fifo
FLAG=.complete
FLIST=./project.list
PRODREPO=bunimsvr
PID=0

function _wait {
if [ $PID -ne 0 ] && kill -0 $PID >/dev/null 2>&1; then
  kill -TERM $PID
  wait $PID
fi
exit 1
}

trap '_wait' 0 TERM INT QUIT

if [ ! -d /aha/fs ]; then
  mkdir -p /aha
  if ! mount -v ahafs /aha /aha; then
    echo "mount ahafs error" >&2
    exit 1
  fi
fi

cat $FLIST | while read files; do
  filesdir=`dirname $files`
  mkdir -p $filesdir
  touch $files
done

mknod $FPIPE p 2>/dev/null

/nim/filesmon/filesmon -L $FLIST -l ./polling.log &
PID=$!

sleep 1
if ! ps -ef | grep $PID | grep -v grep >/dev/null 2>&1; then
  echo "start filesmon error" >&2
  exit 1
fi

kill -TERM $PID
wait $PID

nohup /nim/filesmon/filesmon -L $FLIST -l ./polling.log > $FPIPE &
PID=$!
echo "filesmon started: $PID"

while :
do

  read FILES <$FPIPE
  echo "Caught the event of $FILES, please wait"

  FOLDER=`dirname $FILES`

  echo $FOLDER | IFS='/' read x y code environ host
  cd $FOLDER
  [ -e ./$FLAG ] && cat ./$FLAG | sed '/^$/d' | while read file md5; do
    echo $file | IFS='/' read x y z release rem
    test $DEBUG && echo release=$release
    rd=$FOLDER/$release
    chkfile=$rd/${file#*$release/}
    test $DEBUG && echo $chkfile
    if [ ! -e $chkfile ]; then
      echo "$chkfile no exist" >&2
      touch $rd/_ERROR
      continue
    fi
    chksum=`openssl dgst -md5 <$chkfile 2>/dev/null | awk '{print $NF}'`
    if [ -z "$chksum" -o "$chksum" != "$md5" ]; then
      chksum=${chksum:-"null"}
      echo "$checkfile checksum failed: record is $md5, but check is $chksum" >&2
      touch $rd/_ERROR
    fi
  done
  cd $OLDPWD
  if [ "$environ" != "prod" ] || [ "$environ" = "prod" -a "`hostname`" = "$PRODREPO" ]; then
    test $DEBUG && echo \
    /usr/bin/rsync -av --delete -e ssh --rsync-path="mkdir -p /${environ}/ && rsync" ${FOLDER%/}/ root@${host}:/${environ}/

    /usr/bin/rsync -av --delete -e ssh --rsync-path="mkdir -p /${environ}/ && rsync" ${FOLDER%/}/ root@${host}:/${environ}/
  else
    test $DEBUG && echo \
    /usr/bin/rsync -av --delete -e ssh --exclude="$FLAG" --rsync-path="mkdir -p /project/$code/prod/$host/ && rsync" ${FOLDER%/}/ root@${PRODREPO}:/project/$code/prod/$host/

    /usr/bin/rsync -av --delete -e ssh --exclude="$FLAG" --rsync-path="mkdir -p /project/$code/prod/$host/ && rsync" ${FOLDER%/}/ root@${PRODREPO}:/project/$code/prod/$host/

    test $DEBUG && echo \
    /usr/bin/rsync -av --delete -e ssh ${FOLDER%/}/$FLAG root@${PRODREPO}:/project/$code/prod/$host/

    /usr/bin/rsync -av --delete -e ssh ${FOLDER%/}/$FLAG root@${PRODREPO}:/project/$code/prod/$host/
  fi
done

wait $PID
