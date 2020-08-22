#!/bin/bash

# count number of failed test cases
failures=0

# score tracking
earnedPoints=0
availablePoints=0

# set up trap so we can kill process after it starts running with ctrl-C (work around for using timeout)
trap 'kill -s INT $pid ; exit' INT


# make the project first
if make
then

  # remove the last output folder
  rm -r output

  # read test cases line by line
  while read -r testCase ; do
    testCaseArray=(${testCase})

    # get test case parameters
    testCaseName="${testCaseArray[0]}"
    testCasePoints="${testCaseArray[1]}"
    testMaxTime="${testCaseArray[2]}"
    outputDir="${testCaseArray[3]}"
    compareFile="${testCaseArray[4]}"
    command="${testCaseArray[*]:5}"

    # ensure output directories exist
    mkdir -p "${outputDir}"
    mkdir -p "${outputDir}/diffs"
    mkdir -p "${outputDir}/out"

    # some output files
    outputFile=${outputDir}${testCaseName}
    outFile="${outputDir}out/${testCaseName}"
    diffFile="${outputDir}diffs/${testCaseName}"

    # replace 'OUTPUT_FILE' in command string with the expected output file
    command="${command/OUTPUT_FILE/${outputFile}}"

    echo "Running test ${testCaseName}:"
    echo "  ${command}"

    # run command (with time limit)
    testCommand="{ eval \"${command}\" ; } &> ${outFile}"
    timeout "${testMaxTime}" bash -c "${testCommand}" &
    pid=$!
    wait $pid
    timedout=$?

    failed=1
    
    if [[ "${outputFile}" == *mr-grep* ]]
    then
        sort -t' ' -k1 -n "${outputFile}"  > "${outputFile}Sorted"
        cat "${outputFile}Sorted" > ${outputFile}
    fi

    # check if command timed out
    if [ "${timedout}" -eq 124 ]
    then
      # timed out -> failure
      echo "  Test ${testCaseName} failed:"
      echo "    Timed out."
      failed=1

    # check if output file was actually created
    elif [ ! -f "${outputFile}" ]
    then
      # no output -> failure
      echo "  Test ${testCaseName} failed:"
      echo "    No output."
      failed=1

    # check if the output file matches the expected output
    elif diff "${outputFile}" "${compareFile}" > "${diffFile}"
    then
      # match -> success
      echo "  Test ${testCaseName} passed"
      failed=0
    else
      # no match -> failure
      echo "  Test ${testCaseName} failed:"
      echo "    Incorrect output. Check diff file: ${diffFile}"
      failed=1
    fi

    # update failure and score tracking
    let availablePoints+=testCasePoints

    if [ "${failed}" -eq 1 ]
    then
      let failures++
    else
      let earnedPoints+=testCasePoints
    fi

  done < ./input/test-cases # feed in the test case file to the loop


  # check for any failures and output
  if [ "${failures}" -eq 0 ]
  then
    echo "All tests passed"
  else
    echo "Failures: ${failures}"
  fi

else
  # make failed, do not even bother running
  echo "Make failed"
fi

echo "Score: ${earnedPoints}/${availablePoints}"
echo "Score represents ${availablePoints} pts of the 100 pts available in this lab."
