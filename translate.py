import os
import re
import sys
import tempfile

from google.cloud import translate

translate_client = translate.Client()

def translate(match):
    text = match.group(1)
    translation = translate_client.translate(
        text,
        target_language='en')
    return '/* {} */'.format(translation['translatedText'])

pattern = re.compile(r'/\*(.*?)\*/', re.S)

with tempfile.NamedTemporaryFile(dir='.', delete=False, mode='w') as temp:
    with open(sys.argv[1], 'r') as f:
        text = f.read()
        text_en = re.sub(pattern, translate, text)
        temp.write(text_en)

os.replace(temp.name, sys.argv[1])
