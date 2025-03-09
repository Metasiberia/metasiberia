/*=====================================================================
AudioFileReader.cpp
-------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "AudioFileReader.h"


#include "MP3AudioFileReader.h"
#include "WavAudioFileReader.h"
#include "../utils/Exception.h"
#include "../utils/ConPrint.h"
#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"


namespace glare
{


SoundFileRef AudioFileReader::readAudioFile(const std::string& path)
{
	if(
		::hasExtension(path, "mp3") || 
		::hasExtension(path, "wav"))
	{
		MemMappedFile file(path);
		return readAudioFileFromBuffer(path, ArrayRef<uint8>((const uint8*)file.fileData(), file.fileSize()));
	}
	else
		throw glare::Exception("Unhandled audio format: " + ::getExtension(path));
}


SoundFileRef AudioFileReader::readAudioFileFromBuffer(const std::string& path, ArrayRef<uint8> audio_data_buf)
{
	// NOTE: Using WMFVideoReader (windows media foundation) for audio file reading was causing the rendering to stutter - something in nvopengl.dll was waiting for something the WMF was doing
	// when decoding an Mp3.
	// minimp3 is way faster than WMF also.

	if(::hasExtension(path, "mp3"))
	{
		return MP3AudioFileReader::readAudioFile(path);
	}
	else if(::hasExtension(path, "wav"))
	{
		return WavAudioFileReader::readAudioFile(path);
	}
	else
		throw glare::Exception("Unhandled audio format: " + ::getExtension(path));
}


} // end namespace glare


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/CircularBuffer.h"
#include "../utils/Mutex.h"
#include "../utils/Lock.h"
#include "../utils/ThreadSafeQueue.h"
#include "../utils/ConPrint.h"
#include "../utils/Timer.h"
#include "../utils/FileUtils.h"


void glare::AudioFileReader::test()
{
	conPrint("AudioFileReader::test()");

	try
	{
		/*{
			const std::vector<std::string> paths = FileUtils::getFilesInDirWithExtensionFullPaths("D:\\audio\\substrata_mp3s", "mp3");
			for(size_t i = 0; i<paths.size(); ++i)
			{
				//conPrint("Loading file " + paths[i] + "...");
				Timer timer;
				SoundFileRef sound_file = AudioFileReader::readAudioFile(paths[i]);
				testAssert(sound_file->num_channels == 2);
				testAssert(sound_file->sample_rate == 44100 || sound_file->sample_rate == 48000);
				conPrint(timer.elapsedStringNSigFigs(4) + ": " + paths[i]);
			}
		}*/


		// AAC (m4a)
		/*{
			AudioFileContent content;
			AudioFileReader::readAudioFile(TestUtils::getTestReposDir() + "/testfiles/aac/sample.aac", content);
			testAssert(content.num_channels == 2);
			testAssert(content.sample_rate_hz == 44100);
		}*/

	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	conPrint("AudioFileReader::test() done.");
}


#endif // BUILD_TESTS
