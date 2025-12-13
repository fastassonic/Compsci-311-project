from uuid import uuid4
from datetime import datetime
from nicegui import ui, app
import os
from collections import deque
import subprocess
import errno
import time
# this is just storing all the messages for now
# i need help replacing this with the real backend
#messages = []  # each message is a dict with user + text + time

# just gets the current time for the message
def timestamp() -> str:
    return datetime.now().strftime('%I:%M %p').lstrip('0')





@ui.page('/')
def index():
    @ui.refreshable
    def chat_messages(own_id: str,messages,backendpipe):
        # loop through recent messages and display them
        if backendpipe.closed:
            return
        #This use of an fd is to use a nonblock flag
        try:
            while True:
                #This is sorta hacky but this prevents a block
                line = backendpipe.readline()
                if not line:  # no data right now
                    break
                line = line.strip()
                messages.append(line)
        except BlockingIOError:
            pass
        
        #given it's now a queue with maxlenght of 200
        for m in messages:
            # this logic will have to be entirely reworked
            msgid = m.split(":",1)[0]
            msg = m.split(":",1)[1]
            
            if len(msg.split(":",1)) >=2:
                name = msg.split(":",1)[0]
                msg = msg.split(":",1)[1]
            else:
                name="Server/Undefined"
            #reduced to bare essentials for our own sake
            ui.chat_message(
                text=msg,
                sent=(msgid == own_id),  # checks if its your message
                name=('You' if msgid == own_id else name),
            )
        
    # moving it into index so it's locally scoped per client
    messages = deque(maxlen=200)
    # keeps track of each user separately per browser
    session = app.storage.user

    # give the user an id if they dont have one yet
    if 'user_id' not in session:
        session['user_id'] = str(uuid4())[:8]

    # default username
    if 'name' not in session:
        session['name'] = f'User-{session["user_id"]}'

    user_id = session['user_id'] 
    print(f"Client {user_id} connected.")

    #initialize the named pipe
    pipe_path = "/tmp/"+str(user_id)
    try:
        os.mkfifo(pipe_path+"_fromClient")
    except FileExistsError:
        print(f"Error intilizing pipe {pipe_path}, does it allready exist?")


    try:
        os.mkfifo(pipe_path+"_toClient")
    except FileExistsError:
        print(f"Error intilizing pipe {pipe_path}, does it allready exist?")
    
    Process = subprocess.Popen(["../client/Client",user_id],stdout=None, stderr=None)
    print(Process.pid)
    while True:
        try:
            writefd = os.open(pipe_path+"_fromClient", os.O_WRONLY | os.O_NONBLOCK)
            writingbackendpipe = os.fdopen(writefd, "w")
            break
        except OSError as e:
            if e.errno == errno.ENXIO:
                # no reader yet
                time.sleep(0.05)
            else:
                raise

    
    readfd = os.open(pipe_path+"_toClient", os.O_RDONLY | os.O_NONBLOCK)
    readbackendpipe = os.fdopen(readfd)

    # small styling so it looks nicer
    ui.add_css('''
    .chat-wrap { max-width: 900px; margin: 0 auto; }
    .nicegui-content { background: #f6f7fb; }
    ''')

    with ui.column().classes('chat-wrap w-full'):
        # top bar
        #right now this is useless, I cna probbaly implement this super easily*
        with ui.row().classes('w-full items-center justify-between q-pa-sm'):
            ui.label('CMPSC 311Chat Room ').classes('text-2xl font-bold')

            # lets user change their display name
            with ui.row().classes('items-center'):
                name_input = ui.input(
                    'enter display name:',
                    value=session['name']
                ).props('dense outlined')

                ui.button(
                    'Update',
                    on_click=lambda: session.__setitem__('name', name_input.value)
                ).props('outline')

        ui.separator()

        # main chat area
        with ui.card().classes('w-full').style('height: 65vh; overflow-y: auto;'):
            with ui.column().classes('w-full'):
                chat_messages(user_id,messages,readbackendpipe)

        # message input area
        with ui.row().classes('w-full items-center q-pa-sm'):
            text = ui.input(
                placeholder='Type a message (Max Length is 1500 characters)'
            ).props('outlined').classes('flex-grow')

            # send button logic
            def send():
                msg = (text.value or '').strip()
                if not msg:
                    return

                # add message to list (ui-only for now)
                """
                messages.append({
                    'user_id': user_id,
                    'name': session['name'],
                    'text': msg,
                    'time': timestamp(),
                })
                """
                if len(msg) > 1500:
                    messages.append("Client:Client:This messsage is over our perfered character length, please go back and split up your message to accomodate for a fixed character length.")
                text.value = ''
                #this will block with nobody to read it but-
                #Honestly, if somehow the backend crashes something is wrong
                writingbackendpipe.write(f"{user_id}:{session['name']}:{msg}\n")
                writingbackendpipe.flush()
                    

            ui.button('Send', on_click=send).props('unelevated color=primary')
            text.on('keydown.enter', lambda _: send())

        # refreshes so multiple tabs update
        ui.timer(0.5, lambda: chat_messages.refresh(user_id,messages,readbackendpipe))
        def cleanup():
            print(f"Client {user_id} disconnected, removing self from pipe")
            try:
                readbackendpipe.close()
                writingbackendpipe.close()
            except Exception:
                pass
            try:
                Process.terminate()
            except Exception:
                pass
        ui.context.client.on_disconnect(cleanup)


# runs the nicegui app
ui.run(
    title='Chat Frontend',
    reload=False,
    storage_secret='dev-secret-key'
)
