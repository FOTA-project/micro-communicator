import time
import pyrebase

# upload
firebaseConfig = {
    "apiKey": "AIzaSyBgBFhNa6OnJCLbFTQW3vF_Cyz-rMyN4vU",
    "authDomain": "fota-server-b4148.firebaseapp.com",
    "databaseURL": "https://fota-server-b4148.firebaseio.com",
    "projectId": "fota-server-b4148",
    "storageBucket": "fota-server-b4148.appspot.com",
    "messagingSenderId": "774423425890",
    "appId": "1:774423425890:web:f506832444c3d30b2c323b",
    "measurementId": "G-2DE9D9TN6N"
  };

email = r"fota_project_gp_iti@gmail.com"
password = r"12345@ITI"

# Initialize Firebase
firebase = pyrebase.initialize_app(firebaseConfig);
auth = firebase.auth()
user = auth.sign_in_with_email_and_password(email, password)
user = auth.refresh(user['refreshToken']) # optional

db = firebase.database()
storage = firebase.storage()

user_uid = user['userId']
user_tokenId = user['idToken']

user_db_dir = "users/" + user_uid + "/STM32"
isTerminate = 0

INSTRUCTION_GET_PROGRESS_FLAG     = -1
#INSTRUCTION_COMM_TIMEOUT          = -2
INSTRUCTION_TERMINATE_ON_SUCCESS  = -3
INSTRUCTION_WRITE_MAX_REQUESTS    = -4
#INSTRUCTION_GET_PROGRESS_FLAG_ARB = -5

elfProgress = db.child(user_db_dir + "/elfProgress").get(user_tokenId).val()

progressInstructionFile = open('progress.txt', 'rb')
progressInstructionFile.seek(0, 0)

while isTerminate == 0:
    line = progressInstructionFile.readline().decode('utf-8').strip()
    #print("line = %s\n" %(line))
    
    if line == '':
        time.sleep(0.001 * 50) # 50ms
        #print("line is empty")
        #progressInstructionFile.seek(-len(prevLine), 2)
        continue
    
    if int(line[:2], 10) == INSTRUCTION_TERMINATE_ON_SUCCESS:
        isTerminate = 1
        #print("isTerminate = 1\n")
    elif int(line[:2], 10) == INSTRUCTION_WRITE_MAX_REQUESTS:
        db.child(user_db_dir).update({"elfProgressMaxRequest" : int(line.split()[1], 10)}, user_tokenId)
        #print("INSTRUCTION_WRITE_MAX_REQUESTS, %d\n" %(int(line.split()[1], 10)))
    elif int(line[:2], 10) == INSTRUCTION_GET_PROGRESS_FLAG:
        # goto position of requests count, and fill it from server
        #print("writing old progress = %d\n" %(elfProgress))
        lastProgressFile = open('last-progress.txt', 'r+')
        lastProgressFile.seek(0)
        lastProgressFile.write("00%d\n" %(elfProgress))
        #print("done\n")
        lastProgressFile.close()

        
    else: # normal +ve number
        db.child(user_db_dir).update({"elfProgress" : int(line, 10)}, user_tokenId)
        #print("normal number, %d\n" %(int(line, 10)))

# end of while
    
#### done
print("done...\n")
