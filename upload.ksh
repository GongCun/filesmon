#!/usr/bin/ksh

REPO=qunimsvr
PASS=/etc/.project.pass
APP=`hostname | awk '{print substr($0, 3, 3)}'`

[ $# -ne 3 ] && {
  echo "Usage: `basename $0` [release] [host] [uat|pre|prod]" >&2
  exit 1
}

RELEASE=$1
HOST=$2
ENVIRON=$3

FOLDER=/to_${ENVIRON}/$HOST/

host $REPO >/dev/null 2>&1 || {
  echo "Host name $REPO does not exist"
  exit 1
}
[ ! -d ${FOLDER%/}/$RELEASE -o ! -e $PASS ] && {
  echo "$FOLDER or $PASS does not exist" >&2
  exit 1
}

test $DEBUG && echo /usr/bin/rsync -av --delete --password-file=$PASS --exclude=".complete" $FOLDER project@${REPO}::project/${APP}/${ENVIRON}/${HOST}/
/usr/bin/rsync -av --delete --password-file=$PASS --exclude=".complete" $FOLDER project@${REPO}::project/${APP}/${ENVIRON}/${HOST}/

COMPLETE=${FOLDER%/}/.complete
echo $COMPLETE
>$COMPLETE
find ${FOLDER%/}/$RELEASE ! -name ".complete" -a -type f | while read file; do
  md5=`openssl dgst -md5 <$file | awk '{print $NF}'`
  echo "$file $md5" >>$COMPLETE
done

test $DEBUG && echo /usr/bin/rsync -av --delete --password-file=$PASS $COMPLETE project@${REPO}::project/${APP}/${ENVIRON}/${HOST}/
/usr/bin/rsync -av --delete --password-file=$PASS $COMPLETE project@${REPO}::project/${APP}/${ENVIRON}/${HOST}/

