pipeline {
    agent {
        dockerfile {
            label 'scsitestbenchblr'
            filename 'Dockerfile'
            args '--network=none -u 0:200'
        }
    }
    stages {
        stage('Build') {
            steps {
                sh './build.sh'
            }
        }
        stage('Deploy'){
            steps{
                archiveArtifacts artifacts: 'mini_sandbox/src/main/tools/out/'
            }
        }
    }
}
