API_URL="https://girder.hub.yt/api/v1"
FILE_ID="66ea79a6999605c485c8d620"
LOCAL_FILE="Zoomin_IC"

# download
girder-cli --api-url ${API_URL} download --parent-type item ${FILE_ID} ${LOCAL_FILE}

# unzip
tar zxvf ${LOCAL_FILE}/Zoomin_IC_highres.tar.gz
rm -r ${LOCAL_FILE}
