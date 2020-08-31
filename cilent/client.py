
import os
import socket
"""
REQUESTS
"""
START_SEND_FILE = '\x01'
NEXT_SEND_FILE = '\x02'
DOWNLOAD_FILE = '\x03'
GET_PATH_INFO = '\x04'
GET_FILE_INFO = '\x05'
CHANGE_DIRECTORY = '\x06'
DELETE_FILE = '\x07'

RESP_OK = '\x00'
RESP_ERR = '\x01'
RESP_MORE = '\x02'

class Client:

    def __init__(self, user=''):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind((socket.gethostbyname(socket.gethostname()),20000))
        self.SERVER_ADDRESS = ("192.168.18.254", 20000)
        self.HEADER_SIZE = 3
        self.USER = user
        self.ID = '\x00'
        self.user_dir = os.getcwd() #+ "\\" + user
        self.remote_dir_path = '/'


    def show_local_files(self):
        files = os.listdir()
        for file,x in zip(files, range(1, len(files)+1)):
            print(file, end='   ')
            if x % 5 == 0:
                print('', end='\n')
        print('')

    def show_local_directory(self):
        print(os.getcwd())

    def send_and_recv_packet(self, operation, data='', id='\x00', answer='\x00' ):
        it = 0
        recv_packet = None
        while recv_packet is None and it < 10:
            try:
                packet = (operation + answer + id + data).encode('utf-8')
                #print(packet)
                self.socket.sendto(packet, self.SERVER_ADDRESS)
                self.socket.settimeout(0.2)
                packet, address = self.socket.recvfrom(1024)
                if address != self.SERVER_ADDRESS:
                    continue
                return packet
            except socket.timeout:
                it += 1
        print("Failed to connect")
        return bytes(''.encode("utf-8"))


    def get_remote_directory(self):
        print(self.remote_dir_path)


    def get_remote_fileinfo(self):
        packet = self.send_and_recv_packet(GET_FILE_INFO, data=self.remote_dir_path).decode('utf-8')
        if packet is '':
            return
        if packet[0] == GET_FILE_INFO and packet[1] == RESP_OK:
            files = packet[3:].split(sep='*')
            for file in files:
                print(file)
        if packet[1] != RESP_OK:
            print("Server returned error")


    def change_local_directory(self, path):
        try:
            os.chdir(path)
        except FileNotFoundError:
            print("Can't find directory")


    def change_remote_directory(self, path='/'):
        if path == '/':
            self.remote_dir_path = '/'
            return

        if path == '..':
            if path != '/':
                path = self.remote_dir_path.split(sep='/')
                self.remote_dir_path = '/'.join(path[:-1])
                if self.remote_dir_path == '':
                    self.remote_dir_path = '/'
                print(self.remote_dir_path)
            return

        if path[0] != '/':
            if self.remote_dir_path == '/':
                path = self.remote_dir_path + path
            else:
                path = self.remote_dir_path + '/' + path
        packet = self.send_and_recv_packet(CHANGE_DIRECTORY, path)
        packet = packet.decode('utf-8')
        if packet[0] == CHANGE_DIRECTORY and packet[1] == RESP_OK:
            self.remote_dir_path = path
            print(self.remote_dir_path)
        elif packet[1] == CHANGE_DIRECTORY and packet[1] == RESP_ERR:
            print("Server: Invalid path")
        else:
            print("Network Error... Try again")

    def send_file(self, filename):
        try:
            with open(filename, 'r') as file:
                if self.remote_dir_path == '/':
                    path = self.remote_dir_path + filename
                else:
                    path = self.remote_dir_path + '/' + filename
                freebuffer = 512 - self.HEADER_SIZE - len(path) - 1
                msg_buffer = file.read(freebuffer)
                packet = self.send_and_recv_packet(START_SEND_FILE, path + '*' + msg_buffer).decode()
                if packet[0] == START_SEND_FILE and packet[1] == RESP_OK:
                    pass
                else:
                    print("Error during upload...")
                msg_buffer = file.read(freebuffer)
                while msg_buffer:
                    packet = self.send_and_recv_packet(NEXT_SEND_FILE, path + "*" + msg_buffer).decode('utf-8')
                    if packet[0] == NEXT_SEND_FILE and packet[1] == RESP_OK:
                        pass
                    else:
                        print("Error during upload.")
                        #delete file in dbase
                        break
                    msg_buffer = file.read(freebuffer)
            if packet[1] == RESP_OK:
                print("File sent")
        except FileNotFoundError:
            print("File does not exist.")


    def download_file(self, filename):
        with open(filename, 'w') as file:
            packet = self.send_and_recv_packet(DOWNLOAD_FILE).decode('utf-8')
            if packet[0] != DOWNLOAD_FILE or packet[1] == RESP_ERR:
                print("Network Error!")
                file.close()
                return
            file.write(packet[3:])
            while packet[1] == RESP_MORE:
                packet, address = self.socket.recvfrom(1024)


    def delete_file(self, filename):
        path =''
        if self.remote_dir_path == '/':
            path = self.remote_dir_path + filename
        else:
            path = self.remote_dir_path + '/' + filename
        packet = self.send_and_recv_packet(DELETE_FILE, path).decode('utf-8')
        if packet[0] != DELETE_FILE or packet[1] == RESP_ERR:
            print("Network Error")
        else:
            print("File has been deleted.")







if __name__ == '__main__':
    client = Client()
    while True:
        command = input(">>").split()
        command[0] = command[0].upper()
        if command[0] == 'HELP':
            print("Commands:\n",
                  "File upload/download:             SEND   GET \n",
                  "List local/remote files:          LF     RF \n",
                  "Current local/remote directory:   CWD    CWRD \n",
                  "Change local/remote directory:    CD     CRD \n",
                  "Delete file/leave program:        DEL    EXIT \n",
                  )
        elif command[0] == 'CWD':
            client.show_local_directory()
        elif command[0] == 'LF':
            client.show_local_files()
        elif command[0] == 'RF':
            client.get_remote_fileinfo()
        elif command[0] == 'CWRD':
            client.get_remote_directory()
        elif command[0] == 'CD':
            try:
                client.change_local_directory(command[1])
            except IndexError:
                print("Specify new path")
        elif command[0] == 'CRD':
            try:
                client.change_remote_directory(command[1])
            except IndexError:
                client.change_remote_directory('/')
        elif command[0] == 'SEND':
            try:
                client.send_file(command[1])
            except IndexError:
                print("Give filename")
        elif command[0] == 'DEL':
            try:
                client.delete_file(command[1])
            except IndexError:
                print("Give filename")
        elif command[0] == "EXIT":
            break
