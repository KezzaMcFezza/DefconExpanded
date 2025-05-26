//DefconExpanded, Created by...
//KezzaMcFezza - Main Developer
//Nexustini - Server Managment
//
//Notable Mentions...
//Rad - For helping with python scripts.
//Bert_the_turtle - Doing everthing with c++
//
//Inspired by Sievert and Wan May
// 
//Last Edited 14-04-2025

import { customAlert, customConfirm } from '../main/popup.js';

let isEditingProfile = false;
let hasChangedData = false;
let eventListenersInitialized = false;

function initializeProfileEditing() {
    const editButton = document.getElementById('edit-profile-btn');
    const editButtonMobile = document.getElementById('edit-profile-btn-mobile');
    const editableFields = document.querySelectorAll('.editable');
    const editableLists = document.querySelectorAll('.editable-list');

    if (editButton) {
        editButton.addEventListener('click', toggleEdit);
    }

    if (editButtonMobile) {
        editButtonMobile.addEventListener('click', toggleEdit);
    }

    function toggleEdit() {
        if (isEditingProfile) {
            const shouldSave = hasChangedData ? 
                confirm('Save your changes?') : 
                true; 
            
            if (shouldSave) {
                saveChanges();
            } else {
                exitEditMode(false);
                return;
            }
        }

        isEditingProfile = !isEditingProfile;
        hasChangedData = false;
        
        document.body.classList.toggle('editing');
        
        if (editButton) {
            editButton.classList.toggle('editing');
            editButton.innerHTML = isEditingProfile ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (editButtonMobile) {
            editButtonMobile.classList.toggle('editing');
            editButtonMobile.innerHTML = isEditingProfile ?
                '<i class="fas fa-save"></i> Save Changes' :
                '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        const profileBannerOverlay = document.getElementById('profile-banner-overlay');
        const profilePictureOverlay = document.getElementById('profile-picture-overlay');
        
        if (profileBannerOverlay) {
            profileBannerOverlay.style.display = isEditingProfile ? 'flex' : 'none';
        }
        
        if (profilePictureOverlay) {
            profilePictureOverlay.style.display = isEditingProfile ? 'flex' : 'none';
        }

        if (isEditingProfile) {
            setTimeout(enableEditing, 0);
        }
        
        editableFields.forEach(field => {
            if (isEditingProfile) {
                field.setAttribute('data-editing', 'true');
            } else {
                field.removeAttribute('data-editing');
            }
        });
    }

    function enableEditing() {
        editableFields.forEach(field => {
            field.contentEditable = true;
            
            field.classList.add('temp-class');
            void field.offsetHeight; 
            field.classList.remove('temp-class');
            
            field.addEventListener('input', () => {
                hasChangedData = true;
            });
        });

        editableLists.forEach(list => {
            const existingBtn = list.parentNode.querySelector('.add-item-btn');
            if (existingBtn) existingBtn.remove();
            
            const addItemBtn = document.createElement('button');
            addItemBtn.textContent = 'Add Item';
            addItemBtn.classList.add('add-item-btn');
            addItemBtn.addEventListener('click', (e) => {
                e.preventDefault();
                addListItem(list);
                hasChangedData = true;
            });
            list.parentNode.insertBefore(addItemBtn, list.nextSibling);

            list.querySelectorAll('li').forEach(item => {
                const existingDeleteBtn = item.querySelector('.delete-item-btn');
                if (existingDeleteBtn) existingDeleteBtn.remove();
                addDeleteButton(item);
            });
        });
    }

    function addListItem(list) {
        const newItem = document.createElement('li');
        const textSpan = document.createElement('span');
        textSpan.contentEditable = true;
        textSpan.textContent = 'New item';
        textSpan.addEventListener('input', () => {
            hasChangedData = true;
        });
        newItem.appendChild(textSpan);
        addDeleteButton(newItem);
        list.appendChild(newItem);
    }

    function addDeleteButton(item) {
        const deleteBtn = document.createElement('button');
        deleteBtn.innerHTML = '<i class="fas fa-times"></i>';
        deleteBtn.classList.add('delete-item-btn');
        deleteBtn.addEventListener('click', () => {
            item.remove();
            hasChangedData = true;
        });
        item.appendChild(deleteBtn);
    }

    function exitEditMode(saveChangesMode = true) {
        isEditingProfile = false;
        hasChangedData = false;
        
        document.body.classList.remove('editing');
        
        if (editButton) {
            editButton.classList.remove('editing');
            editButton.innerHTML = '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        if (editButtonMobile) {
            editButtonMobile.classList.remove('editing');
            editButtonMobile.innerHTML = '<i class="fas fa-pencil-alt"></i> Edit Profile';
        }

        const profileBannerOverlay = document.getElementById('profile-banner-overlay');
        const profilePictureOverlay = document.getElementById('profile-picture-overlay');
        
        if (profileBannerOverlay) {
            profileBannerOverlay.style.display = 'none';
        }
        
        if (profilePictureOverlay) {
            profilePictureOverlay.style.display = 'none';
        }

        editableFields.forEach(field => {
            field.contentEditable = false;
            field.removeAttribute('data-editing');
        });

        editableLists.forEach(list => {
            const addItemBtn = list.parentNode.querySelector('.add-item-btn');
            if (addItemBtn) addItemBtn.remove();
            list.querySelectorAll('.delete-item-btn').forEach(btn => btn.remove());
        });
    }

    async function saveChanges() {
        try {
            const currentUserResponse = await fetch('/api/current-user');
            const currentUserData = await currentUserResponse.json();
            const profileUsername = document.getElementById('profile-username-label').textContent;

            if (!currentUserData.user || currentUserData.user.username !== profileUsername) {
                await customAlert('You are not authorized to edit this profile.');
                location.reload();
                return;
            }

            const profileData = {
                discord_username: document.getElementById('discord-info').textContent,
                steam_id: document.getElementById('steam-info').textContent,
                bio: document.getElementById('bio-text').textContent,
                defcon_username: document.getElementById('defcon-username').textContent,
                years_played: document.getElementById('years-of-service').textContent,
                main_contributions: Array.from(document.getElementById('main-contributions-list').children).map(li => li.textContent.replace('X', '').trim()),
                guides_and_mods: Array.from(document.getElementById('guides-and-mods-list').children).map(li => li.textContent.replace('X', '').trim())
            };

            const loadingIndicator = document.createElement('div');
            loadingIndicator.classList.add('loading-overlay');
            loadingIndicator.innerHTML = '<div class="spinner"><i class="fas fa-circle-notch fa-spin"></i></div>';
            document.body.appendChild(loadingIndicator);

            const response = await fetch('/api/update-profile', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(profileData),
            });

            const result = await response.json();

            if (document.body.contains(loadingIndicator)) {
                document.body.removeChild(loadingIndicator);
            }

            if (result.success) {
                await customAlert('Profile updated successfully!');
            } else {
                await customAlert('Failed to update profile. Please try again.');
            }

            exitEditMode(false); 
        } catch (error) {
            console.error('Error updating profile:', error);
            
            const loadingIndicator = document.querySelector('.loading-overlay');
            if (loadingIndicator && document.body.contains(loadingIndicator)) {
                document.body.removeChild(loadingIndicator);
            }
            
            await customAlert('An error occurred while updating the profile.');

            exitEditMode(false);
        }
    }
}

function formatDurationProfile(minutes) {
    if (!minutes || isNaN(minutes)) return '0 min';

    if (minutes < 60) {
        return `${Math.round(minutes)} min`;
    } else {
        const hours = Math.floor(minutes / 60);
        const remainingMinutes = Math.round(minutes % 60);
        return `${hours} hr ${remainingMinutes} min`;
    }
}

function initializeProfileImageEditing() {
    if (eventListenersInitialized) return;
    eventListenersInitialized = true;
    
    if (!window.Cropper) {
        loadCropperJS();
    }

    const profilePictureContainer = document.getElementById('profile-picture-container');
    const profileBannerOverlay = document.getElementById('profile-banner-overlay');
    const profilePictureOverlay = document.getElementById('profile-picture-overlay');
    const profileBanner = document.getElementById('profile-banner');
    const imageUpload = document.getElementById('image-upload');
    
    let currentEditType = null; 
    let cropper = null;
    let cropperModal = document.getElementById('cropper-modal');
    
    if (profilePictureOverlay) {
        profilePictureOverlay.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            if (document.body.classList.contains('editing')) {
                currentEditType = 'profile';
                if (imageUpload) imageUpload.click();
            }
        });
    }

    if (profileBannerOverlay) {
        profileBannerOverlay.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            if (document.body.classList.contains('editing')) {
                currentEditType = 'banner';
                if (imageUpload) imageUpload.click();
            }
        });
    }

    if (imageUpload) {
        imageUpload.addEventListener('change', function(e) {
            if (!e.target.files || !e.target.files[0]) return;
            
            const file = e.target.files[0];
            const reader = new FileReader();
            
            reader.onload = function(event) {
                const tempImg = new Image();
                tempImg.src = event.target.result;
                
                tempImg.onload = function() {
                    const cropperImage = document.getElementById('cropper-image');
                    cropperImage.src = event.target.result;
                    
                    const modalContent = document.querySelector('.cropper-modal-content');
                    const cropperContainer = document.querySelector('.cropper-container');
                    const maxWidth = window.innerWidth * 0.9;
                    const maxHeight = window.innerHeight * 0.8;
                    
                    let modalWidth, modalHeight;
                    
                    if (tempImg.width > maxWidth || tempImg.height > maxHeight) {
                        const widthRatio = maxWidth / tempImg.width;
                        const heightRatio = maxHeight / tempImg.height;
                        const ratio = Math.min(widthRatio, heightRatio);
                        
                        modalWidth = tempImg.width * ratio;
                        modalHeight = tempImg.height * ratio;
                    } else {
                        modalWidth = tempImg.width;
                        modalHeight = tempImg.height;
                    }

                    modalWidth = Math.max(modalWidth, 400);
                    modalHeight = Math.max(modalHeight, 300);

                    cropperContainer.style.padding = '0';

                    modalContent.style.width = 'auto';
                    modalContent.style.maxWidth = `${modalWidth + 40}px`; 
                    
                    cropperModal.style.display = 'flex';
                    
                    if (cropper) {
                        cropper.destroy();
                    }
                    
                    const aspectRatio = currentEditType === 'profile' ? 1 : 3;
                    
                    cropper = new Cropper(cropperImage, {
                        aspectRatio: aspectRatio,
                        viewMode: 2, 
                        dragMode: 'move',
                        background: false, 
                        autoCropArea: 0.9, 
                        responsive: true,
                        restore: false,
                        modal: true, 
                        guides: true,
                        center: true, 
                        highlight: true,
                        cropBoxMovable: true,
                        cropBoxResizable: true,
                        toggleDragModeOnDblclick: false,
                        minContainerWidth: 300,
                        minContainerHeight: 200,
                        ready() {
                            const cropperCanvas = document.querySelector('.cropper-canvas');
                            if (cropperCanvas) {
                                cropperCanvas.style.backgroundImage = 'none';
                                cropperCanvas.style.backgroundColor = '#1c1c2e'; 
                            }
                            
                            if (currentEditType === 'profile') {
                                const containerData = cropper.getContainerData();
                                
                                const size = Math.min(
                                    tempImg.width, 
                                    tempImg.height, 
                                    Math.min(containerData.width, containerData.height) * 0.9
                                );
                                
                                cropper.setCropBoxData({
                                    left: (containerData.width - size) / 2,
                                    top: (containerData.height - size) / 2,
                                    width: size,
                                    height: size
                                });

                                const cropBox = document.querySelector('.cropper-view-box');
                                if (cropBox) {
                                    cropBox.style.borderRadius = '50%';
                                    const face = document.querySelector('.cropper-face');
                                    if (face) {
                                        face.style.borderRadius = '50%';
                                    }
                                }
                            } else {
                                const imageData = cropper.getImageData();
                                const containerData = cropper.getContainerData();
                                const width = Math.min(imageData.width, containerData.width * 0.9);
                                const height = width / aspectRatio;
                                
                                cropper.setCropBoxData({
                                    left: (containerData.width - width) / 2,
                                    top: (containerData.height - height) / 2,
                                    width: width,
                                    height: height
                                });
                            }
                        }
                    });
                };
            };
            
            reader.readAsDataURL(file);
            e.target.value = '';
        });
    }

    document.querySelector('.cropper-close-btn').addEventListener('click', closeCropperModal);
    document.getElementById('cropper-cancel-btn').addEventListener('click', closeCropperModal);
    document.getElementById('cropper-save-btn').addEventListener('click', saveCroppedImage);

    function closeCropperModal() {
        if (cropperModal) {
            cropperModal.style.display = 'none';
        }
        if (cropper) {
            cropper.destroy();
            cropper = null;
        }
    }

    function saveCroppedImage() {
        if (!cropper) return;

        const canvas = cropper.getCroppedCanvas({
            width: currentEditType === 'profile' ? 480 : 1200,
            height: currentEditType === 'profile' ? 480 : 400,
            fillColor: '#000',
            imageSmoothingEnabled: true,
            imageSmoothingQuality: 'high',
        });
        
        canvas.toBlob(async (blob) => {
            try {
                const formData = new FormData();
                formData.append('image', blob, `${currentEditType}.png`);
                formData.append('type', currentEditType);
                
                const response = await fetch('/api/upload-profile-image', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    const cacheBustedUrl = `${result.imageUrl}?t=${new Date().getTime()}`;
                    
                    if (currentEditType === 'profile') {
                        const profilePicture = document.getElementById('profile-picture');
                        if (profilePicture) {
                            profilePicture.src = cacheBustedUrl;
                        }
                    } else if (currentEditType === 'banner') {
                        const profileBanner = document.getElementById('profile-banner');
                        if (profileBanner) {
                            profileBanner.style.backgroundImage = `url(${cacheBustedUrl})`;
                        }
                    }

                    closeCropperModal();
                    
                    await customAlert(`${currentEditType === 'profile' ? 'Profile picture' : 'Banner'} updated successfully!`);
                } else {
                    await customAlert(`Failed to update ${currentEditType === 'profile' ? 'profile picture' : 'banner'}. Please try again.`);
                    closeCropperModal();
                }
            } catch (error) {
                console.error('Error saving image:', error);
                await customAlert(`An error occurred while saving your ${currentEditType === 'profile' ? 'profile picture' : 'banner'}.`);
                
                closeCropperModal();

                document.querySelectorAll('.loading-overlay').forEach(overlay => {
                    if (overlay.parentNode) {
                        overlay.parentNode.removeChild(overlay);
                    }
                });
            }
        }, 'image/png');
    }
}


function loadCropperJS() {
    const cropperCss = document.createElement('link');
    cropperCss.rel = 'stylesheet';
    cropperCss.href = 'https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.5.13/cropper.min.css';
    document.head.appendChild(cropperCss);
    
    const cropperScript = document.createElement('script');
    cropperScript.src = 'https://cdnjs.cloudflare.com/ajax/libs/cropperjs/1.5.13/cropper.min.js';
    document.head.appendChild(cropperScript);
    
    cropperScript.onerror = () => {
        console.error('Failed to load Cropper.js');
        customAlert('Failed to load image editor. Please try again later.');
    };
}

export { 
    initializeProfileEditing, 
    initializeProfileImageEditing, 
    formatDurationProfile
};