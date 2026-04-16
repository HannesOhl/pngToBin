import sys
from PIL import Image


width, height = 400, 400
data = open(sys.argv[1],"rb").read()
img = Image.frombytes("RGBA", (width, height), data)
img.show()
